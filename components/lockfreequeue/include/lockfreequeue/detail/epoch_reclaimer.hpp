#pragma once
#include <atomic>
#include <memory>
#include <cstddef>
#include <cassert>
#include <vector>

#define LF_EBR_ALIGN_CACHE


#if !defined(LIKELY)
#  if defined(__GNUC__) || defined(__clang__)
#    define LIKELY(x)   (__builtin_expect(!!(x), 1))
#    define UNLIKELY(x) (__builtin_expect(!!(x), 0))
#  else
#    define LIKELY(x)   (x)
#    define UNLIKELY(x) (x)
#  endif
#endif


#ifdef LF_EBR_ALIGN_CACHE
#  define LF_EBR_ALIGN alignas(64)
#else
#  define LF_EBR_ALIGN
#endif

namespace lf {

struct DefaultEBRPolicy {
    static constexpr size_t MaxThreads = 128;
    static constexpr size_t RetireThreshold = 64;
    static constexpr size_t OpCheckInterval = 1024;
};

// statistics
struct EBRStats {
    std::atomic<uint64_t> advance_attempts{0};
    std::atomic<uint64_t> advance_success{0};
    std::atomic<uint64_t> retired_count{0};
    std::atomic<uint64_t> reclaimed_count{0};
};

// Per-thread record; aligned to cache line to reduce false sharing
struct LF_EBR_ALIGN ThreadRecord {
    std::atomic<bool> active{false};
    std::atomic<uint64_t> local_epoch{0};
    std::vector<void*> retired_ptrs;
    std::vector<uint64_t> retired_epochs;
    std::vector<void (*)(void*)> retired_deleters;
    uint64_t op_count = 0; // only used in local thread
};

class EpochDomain {
public:
    using Policy = DefaultEBRPolicy;

    EpochDomain(uint64_t start_epoch = 0) noexcept
        : m_global_epoch(start_epoch), m_registered(0) {}

    EpochDomain(const EpochDomain&) = delete;
    EpochDomain& operator=(const EpochDomain&) = delete;

    void enter(size_t tid) noexcept {
        ThreadRecord& rec = m_threads[tid];
        rec.active.store(true, std::memory_order_release);
        rec.local_epoch.store(m_global_epoch.load(std::memory_order_acquire),
                                std::memory_order_release);
    }

    void leave(size_t tid) noexcept {
        m_threads[tid].active.store(false, std::memory_order_release);
    }

    template <typename T, typename Deleter = std::default_delete<T>>
    void retire(T* ptr, Deleter d = {}) {
        if (!ptr) return;
        size_t id = get_tls_thread_id();
        ThreadRecord& rec = m_threads[id];
        uint64_t epoch = m_global_epoch.load(std::memory_order_acquire);
        rec.retired_ptrs.push_back(ptr);
        rec.retired_deleters.push_back(&do_delete<T, Deleter>);
        rec.retired_epochs.push_back(epoch);
        m_stats.retired_count.fetch_add(1, std::memory_order_relaxed);

        if (rec.retired_ptrs.size() >= Policy::RetireThreshold) {
            try_advance_epoch();
            scan_thread(id);
        }

        if (++rec.op_count % Policy::OpCheckInterval == 0) {
            try_advance_epoch();
        }
    }

    // register and cache slot id when thread first use this domain
    size_t get_tls_thread_id() noexcept {
        if (UNLIKELY(s_t_m_last_domain != this)) {
            return slow_get_or_register();
        }
        return s_t_m_last_id;
    }

    void drain_all() {
        for (int round = 0; round < 8; ++round) {
            for (size_t i = 0; i < m_registered.load(std::memory_order_acquire); ++i) {
                scan_thread(i);
            }
        }
        for (size_t i = 0; i < m_registered.load(std::memory_order_acquire); ++i) {
            force_free_thread(i);
        }
    }

    EBRStats& get_domain_stats() noexcept { return m_stats; }

private:

    template <typename T, typename Deleter>
    static void do_delete(void* p) noexcept {
        Deleter{}(static_cast<T*>(p));
    }

    size_t register_thread() noexcept {
        size_t id = m_registered.fetch_add(1, std::memory_order_relaxed);
        assert(id < Policy::MaxThreads && "EpochDomain: exceed MaxThreads");
        m_threads[id].local_epoch.store(m_global_epoch.load(std::memory_order_relaxed),
                                          std::memory_order_relaxed);
        return id;
    }

    size_t slow_get_or_register() noexcept {
        // search for registered domain
        for (auto& b : s_t_m_bindings) {
            if (b.domain == this) {
                s_t_m_last_domain = this;
                s_t_m_last_id = b.id;
                return b.id;
            }
        }

        // register
        size_t id = register_thread();
        s_t_m_bindings.push_back({this, id});
        s_t_m_last_domain = this;
        s_t_m_last_id = id;
        return id;
    }

    void try_advance_epoch() noexcept {
        m_stats.advance_attempts.fetch_add(1, std::memory_order_relaxed);
        if (m_advancing.test_and_set(std::memory_order_acquire)) return;

        uint64_t ge = m_global_epoch.load(std::memory_order_relaxed);
        size_t reg_count = m_registered.load(std::memory_order_acquire);
        for (size_t i = 0; i < reg_count; ++i) {
            ThreadRecord& rec = m_threads[i];
            if (rec.active.load(std::memory_order_acquire)) {
                uint64_t le = rec.local_epoch.load(std::memory_order_acquire);
                if (le < ge) {
                    m_advancing.clear(std::memory_order_release);
                    return;
                }
            }
        }

        if (m_global_epoch.compare_exchange_strong(ge, ge + 1, std::memory_order_acq_rel)) {
            m_stats.advance_success.fetch_add(1, std::memory_order_relaxed);
        }

        m_advancing.clear(std::memory_order_release);
    }

    uint64_t compute_safe_epoch() noexcept {
        uint64_t ge = m_global_epoch.load(std::memory_order_acquire);
        size_t reg_count = m_registered.load(std::memory_order_acquire);
        for (size_t i = 0; i < reg_count; ++i) {
            ThreadRecord& rec = m_threads[i];
            if (rec.active.load(std::memory_order_acquire)) {
                uint64_t le = rec.local_epoch.load(std::memory_order_acquire);
                if (le < ge) {
                    ge = le;
                }
            }
        }
        return ge;
    }

    void scan_thread(size_t tid, bool force = false) noexcept {
        ThreadRecord& rec = m_threads[tid];
        if (rec.retired_ptrs.empty()) return;

        uint64_t se = compute_safe_epoch();

        std::vector<void*> new_ptrs;
        std::vector<uint64_t> new_epochs;
        std::vector<void(*)(void*)> new_deleters;
        size_t retire_count = rec.retired_ptrs.size();
        new_ptrs.reserve(retire_count);
        new_epochs.reserve(retire_count);
        new_deleters.reserve(retire_count);
 
        for (size_t i = 0; i < retire_count; ++i) {
            uint64_t re = rec.retired_epochs[i];
            if (re + 2 <= se || force) {
                void* p = rec.retired_ptrs[i];
                auto d = rec.retired_deleters[i];
                d(p);
                m_stats.reclaimed_count.fetch_add(1, std::memory_order_relaxed);
            } else {
                new_ptrs.push_back(rec.retired_ptrs[i]);
                new_epochs.push_back(rec.retired_epochs[i]);
                new_deleters.push_back(rec.retired_deleters[i]);
            }
        }
        rec.retired_ptrs.swap(new_ptrs);
        rec.retired_epochs.swap(new_epochs);
        rec.retired_deleters.swap(new_deleters);
    }

    void force_free_thread(size_t tid) {
        ThreadRecord& rec = m_threads[tid];
        for (size_t i = 0; i < rec.retired_ptrs.size(); ++i) {
            rec.retired_deleters[i](rec.retired_ptrs[i]);
        }
        rec.retired_ptrs.clear();
        rec.retired_epochs.clear();
        rec.retired_deleters.clear();
    }


private:
    std::atomic<uint64_t>   m_global_epoch{0};
    std::atomic<size_t>     m_registered{0};
    ThreadRecord            m_threads[DefaultEBRPolicy::MaxThreads];
    std::atomic_flag        m_advancing = ATOMIC_FLAG_INIT;
    EBRStats                m_stats{};

    struct Binding {
        EpochDomain* domain;
        size_t id;
    };
    inline static thread_local std::vector<Binding> s_t_m_bindings{};
    inline static thread_local EpochDomain*         s_t_m_last_domain{nullptr};
    inline static thread_local uint64_t             s_t_m_last_id{0};
};

class EpochGuard {
public:
    explicit EpochGuard(EpochDomain& domain) noexcept
        : m_domain(&domain), m_id(m_domain->get_tls_thread_id()) {
        m_domain->enter(m_id);
    }

    ~EpochGuard() {
        m_domain->leave(m_id);
    }

    EpochGuard(const EpochGuard&) = delete;
    EpochGuard& operator=(const EpochGuard&) = delete;

private:
    EpochDomain* m_domain;
    size_t m_id;
};

}; // namespace lf
