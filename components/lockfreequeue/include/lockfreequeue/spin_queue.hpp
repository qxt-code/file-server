#pragma once
#include <type_traits>
#include "detail/backoff.hpp"

namespace lf {

template <typename Queue>
class SpinQueue {
public:

    template<typename... Args>
    SpinQueue(Args&&... args) noexcept : m_q(std::forward<Args>(args)...) {}

    template <typename T>
    bool try_push(T&& v) { return m_q.try_push(std::forward<T>(v)); }

    template <typename T, typename Backoff = ExponentialBackoff>
    bool spin_push(T&& v, unsigned attempts = 128, Backoff bk = Backoff{}) {
        if (m_q.try_push(std::forward<T>(v))) return true;
        for (unsigned i = 0; i < attempts; ++i) {
            bk();
            if (m_q.try_push(std::forward<T>(v))) return true;
        }
        return false;
    }

    template <typename T>
    bool try_pop(T& out) { return m_q.try_pop(out); }

    template <typename T, typename Backoff = ExponentialBackoff>
    bool spin_pop(T& out, unsigned attempts = 128, Backoff bk = Backoff{}) {
        if (m_q.try_pop(out)) return true;
        for (unsigned i = 0; i < attempts; ++i) {
            bk();
            if (m_q.try_pop(out)) return true;
        }
        return false;
    }

    Queue& underlying() noexcept { return m_q; }
private:
    Queue m_q;
};

} // namespace lf
