#pragma once

#include <atomic>
#include <new>
#include <cstddef>
#include <type_traits>
#include <utility>

#include "detail/epoch_reclaimer.hpp"


namespace lf{

constexpr std::size_t CACHE_LINE_SIZE = 64;
    
template<typename _Tp>
class ListMPMCQueue {
public:
    ListMPMCQueue(): 
        _M_head(new Node()), 
        _M_tail(_M_head.load(std::memory_order_relaxed)) {}

    ~ListMPMCQueue() {
        // Ensure all retired nodes are reclaimed before destruction of queue object
        domain.drain_all();
        Node* node = _M_head.load(std::memory_order_relaxed);
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }

    ListMPMCQueue(const ListMPMCQueue&) = delete;
    ListMPMCQueue& operator=(const ListMPMCQueue&) = delete;

    bool try_push(auto&& val) {
        Node* new_node = new Node(std::forward<decltype(val)>(val));
        Node* tail;

        // There is no chance to face ABA problem here, because the new_node is
        // newly created and not in the queue yet.
        for (;;) {
            tail = _M_tail.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);
            // fast fail to avoid redundant fail check
            if (tail == _M_tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (tail->next.compare_exchange_weak(next, new_node,
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
                        _M_tail.compare_exchange_weak(tail, new_node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed);
                        break;
                    }
                } else {
                    _M_tail.compare_exchange_weak(tail, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed);
                }
            }
        }
        return true;
    }

    bool try_pop(_Tp& out) {
        EpochGuard guard(domain);
        Node* head;
        Node* next;
        for (;;) {
            head = _M_head.load(std::memory_order_acquire);
            Node* tail = _M_tail.load(std::memory_order_acquire);
            next = head->next.load(std::memory_order_acquire);
            if (head == _M_head.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (next == nullptr) return false; // empty
                    // assist push: advance tail
                    _M_tail.compare_exchange_weak(tail, next,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed);
                } else {
                    if (_M_head.compare_exchange_weak(head, next,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_relaxed)) {
                        break; // successfully popped head (dummy) -> next becomes new dummy
                    }
                }
            }
        }
        // 'next' now is the first real node containing a value
        out = std::move(*next->value_ptr());
        // destroy the stored value immediately inorder to statistic correctly
        if (next->has_value) {
            std::destroy_at(next->value_ptr());
            next->has_value = false; // now a dummy node
        }
        // old head holds no valid value (dummy), safe to retire
        domain.retire(head);
        return true;
    }

    inline bool empty() const { return _M_head == _M_tail; }

private:
    struct alignas(CACHE_LINE_SIZE) Node {
        std::atomic<Node*> next{nullptr};
        bool has_value{false};
        typename std::aligned_storage<sizeof(_Tp), alignof(_Tp)>::type storage;

        Node() noexcept = default; // dummy node; no value constructed
        template <class V>
        explicit Node(V&& v) : next(nullptr), has_value(true) {
            new (&storage) _Tp(std::forward<V>(v));
        }
        ~Node() {
            if (has_value) {
                std::destroy_at(value_ptr());
            }
        }

        _Tp* value_ptr() { return reinterpret_cast<_Tp*>(&storage); }
        const _Tp* value_ptr() const { return reinterpret_cast<const _Tp*>(&storage); }
    };

    alignas(CACHE_LINE_SIZE) std::atomic<Node*> _M_head;
    alignas(CACHE_LINE_SIZE) std::atomic<Node*> _M_tail;
    EpochDomain domain;
};

}; // namespace lf