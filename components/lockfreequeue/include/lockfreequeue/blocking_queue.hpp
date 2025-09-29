#pragma once

#include <condition_variable>
#include <mutex>
#include <chrono>
#include <optional>
#include "detail/backoff.hpp"
#include "detail/queue_concept.hpp"


namespace lf {

template <typename Queue>
class BlockingQueue {
public:

    template<typename... Args>
    BlockingQueue(Args&&... args) : m_q(std::forward<Args>(args)...) {}

    template <typename T, typename Backoff = ExponentialBackoff>
    bool push(T&& v, Backoff bk = Backoff{}) {
        if (m_q.try_push(std::forward<T>(v))) {
            notify_not_empty();
            return true;
        }
        // spin phase
        for (unsigned i = 0; i < m_cfg.spin_attempts_before_block; ++i) {
            bk();
            if (m_q.try_push(std::forward<T>(v))) {
                notify_not_empty();
                return true;
            }
        }
        // blocking phase
        std::unique_lock<std::mutex> lk(m_mutex);
        // loop for spurious wakeups / contention
        while(!m_q.try_push(std::forward<T>(v))) {
            if (!m_cfg.bounded) {
                // If it's unbounded underlying queue, spinning should have sufficed; but still wait briefly
                m_cv_not_full.wait(lk);
            } else {
                m_cv_not_full.wait(lk);
            }
        }
        lk.unlock();
        notify_not_empty();
        return true;
    }

    template <typename T, typename Rep, typename Period, typename Backoff = ExponentialBackoff>
    bool push_until(T&& v, const std::chrono::duration<Rep,Period>& timeout, Backoff bk = Backoff{}) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        if (m_q.try_push(std::forward<T>(v))) {
            notify_not_empty();
            return true;
        }
        for (unsigned i = 0; i < m_cfg.spin_attempts_before_block; ++i) {
            bk();
            if (m_q.try_push(std::forward<T>(v))) {
                notify_not_empty();
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) return false;
        }
        std::unique_lock<std::mutex> lk(m_mutex);
        while(!m_q.try_push(std::forward<T>(v))) {
            if (m_cv_not_full.wait_until(lk, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        lk.unlock();
        notify_not_empty();
        return true;
    }

    template <typename T, typename Backoff = ExponentialBackoff>
    bool pop(T& out, Backoff bk = Backoff{}) {
        if (m_q.try_pop(out)) { notify_not_full(); return true; }
        for (unsigned i = 0; i < m_cfg.spin_attempts_before_block; ++i) {
            bk();
            if (m_q.try_pop(out)) {
                notify_not_full();
                return true;
            }
        }
        std::unique_lock<std::mutex> lk(m_mutex);
        while(!m_q.try_pop(out)) {
            m_cv_not_empty.wait(lk);
        }
        lk.unlock();
        notify_not_full();
        return true;
    }

    template <typename T, typename Rep, typename Period, typename Backoff = ExponentialBackoff>
    bool pop_until(T& out, const std::chrono::duration<Rep,Period>& timeout, Backoff bk = Backoff{}) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        if (m_q.try_pop(out)) {
            notify_not_full();
            return true;
        }
        for (unsigned i = 0; i < m_cfg.spin_attempts_before_block; ++i) {
            bk();
            if (m_q.try_pop(out)) {
                notify_not_full();
                return true;
            }
            if (std::chrono::steady_clock::now() >= deadline) return false;
        }
        std::unique_lock<std::mutex> lk(m_mutex);
        while(!m_q.try_pop(out)) {
            if (m_cv_not_empty.wait_until(lk, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        lk.unlock();
        notify_not_full();
        return true;
    }

    Queue& underlying() noexcept { return m_q; }

private:
    void notify_not_empty() noexcept {
        m_cv_not_empty.notify_one();
    }
    void notify_not_full() noexcept {
        if (m_cfg.bounded) m_cv_not_full.notify_one();
        else m_cv_not_full.notify_one(); // still notify to wake potential waiters
    }

    struct BlockingConfig {
        unsigned spin_attempts_before_block {128};
        bool bounded { HasCapacity<Queue> }; // whether underlying queue has capacity semantics (used for not_full cv)
    };

    Queue m_q;
    BlockingConfig m_cfg{};
    std::mutex m_mutex;
    std::condition_variable m_cv_not_empty;
    std::condition_variable m_cv_not_full;
};

} // namespace lf
