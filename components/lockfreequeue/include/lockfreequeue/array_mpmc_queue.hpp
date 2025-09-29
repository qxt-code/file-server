#pragma once

#include <atomic>
#include <vector>
#include <new>

namespace lf {

constexpr std::size_t CACHE_LINE_SIZE = 64;

template<typename _Tp>
class ArrayMPMCQueue {
public:
    ArrayMPMCQueue(std::size_t capacity): 
                    _M_C_capacity(align_pow_2(capacity)),
                    _M_C_mask(align_pow_2(capacity) - 1),
                    _M_data(static_cast<Cell*>(::operator new[](align_pow_2(capacity) * sizeof(Cell)))),
                    _M_head(0),
                    _M_tail(0) {
        for (std::size_t i = 0; i < _M_C_capacity; ++i) {
            new (&_M_data[i]) Cell();
            _M_data[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ~ArrayMPMCQueue() {
        while (size()) {
            _Tp tmp;
            try_pop(tmp);
        }
        for (std::size_t i = 0; i < _M_C_capacity; ++i) {
            // If there need call the destructor of _Tp, means the queue is not empty
            // and there are still elements in the queue.
            std::destroy_at(&_M_data[i]);
        }
        ::operator delete[](_M_data);
    }

    ArrayMPMCQueue(const ArrayMPMCQueue&) = delete;
    ArrayMPMCQueue& operator=(const ArrayMPMCQueue&) = delete;

    bool try_push(auto&& val);
    bool try_pop(_Tp& out);

    inline std::size_t capacity() const { return _M_C_capacity; }
    inline std::size_t size() const {
        std::size_t head = _M_head.load(std::memory_order_acquire);
        std::size_t tail = _M_tail.load(std::memory_order_acquire);
        return tail - head;
    }

    inline bool empty() const { return size() == 0; }
    inline bool full() const { return size() == _M_C_capacity; }

private:
    inline std::size_t align_pow_2(std::size_t num) {
        std::size_t ret = 1;
        while (ret < num) {
            ret <<= 1;
        }
        return ret;
    } 

private:
    struct alignas(CACHE_LINE_SIZE) Cell {
        std::atomic<std::size_t> seq;
        std::byte data[sizeof(_Tp)];
        std::byte padding[CACHE_LINE_SIZE - (sizeof(std::atomic<std::size_t>) + sizeof(_Tp)) % CACHE_LINE_SIZE];
    };
    const std::size_t _M_C_capacity;
    const std::size_t _M_C_mask;
    Cell* const _M_data;

    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> _M_head;
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> _M_tail;

};

template<typename _Tp>
bool ArrayMPMCQueue<_Tp>::try_push(auto&& val) {
    Cell* cell;
    std::size_t ticket = _M_tail.load(std::memory_order_relaxed);
    for(;;) {
        cell = &_M_data[ticket & _M_C_mask];
        std::size_t seq = cell->seq.load(std::memory_order_acquire);
        
        // seq == ticket: hasn't been used
        // seq == ticket + 1: has new data, means another thread get ahead
        // seq == ticket - capacity + 1: pre data hasn't been read, means full
        if (seq == ticket) {
            // try to reserve a cell
            if (_M_tail.compare_exchange_weak(ticket, ticket + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed))
                break;
        } else if (seq < ticket) {
            return false;
        } else {
            // get new tail
            ticket = _M_tail.load(std::memory_order_relaxed);
        }
    }

    new (&cell->data) _Tp(std::forward<decltype(val)>(val));
    // use seq = ticket + 1 to represent new data
    cell->seq.store(ticket + 1, std::memory_order_release);
    return true;
}

template<typename _Tp>
bool ArrayMPMCQueue<_Tp>::try_pop(_Tp& out) {
    Cell* cell;
    std::size_t ticket = _M_head.load(std::memory_order_relaxed);
    for (;;) {
        cell = &_M_data[ticket & _M_C_mask];
        std::size_t seq = cell->seq.load(std::memory_order_acquire);
        
        // seq == ticket + 1: has data
        // seq == ticket: empty
        // seq == ticket + capacity: anothor thread get ahead
        if (seq == ticket + 1) {
            // try to reserve data
            if (_M_head.compare_exchange_weak(ticket, ticket + 1,
                                                std::memory_order_relaxed,
                                                std::memory_order_relaxed))
                break;
        } else if (seq == ticket) {
            return false;
        } else {
            ticket = _M_head.load(std::memory_order_relaxed);
        }
    }

    _Tp* ptr = reinterpret_cast<_Tp*>(&cell->data);
    out = std::move(*ptr);
    std::destroy_at(ptr);
    cell->seq.store(ticket + _M_C_capacity, std::memory_order_release);
    return true;
}

}; // namespace lf