#pragma once
#include <list>
#include <unordered_map>
#include <mutex>
#include <optional>

template <typename Key, typename Value>
class LRUCache {
public:
    explicit LRUCache(size_t capacity): capacity_(capacity) {}

    std::optional<Value> get(const Key& k) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(k);
        if (it == map_.end()) return std::nullopt;
        touch(it);
        return it->second.val;
    }

    void put(const Key& k, const Value& v) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(k);
        if (it != map_.end()) {
            it->second.val = v;
            touch(it);
            return;
        }
        if (list_.size() >= capacity_) {
            // evict tail
            auto old_key = list_.back();
            map_.erase(old_key);
            list_.pop_back();
        }
        list_.push_front(k);
        map_[k] = {v, list_.begin()};
    }

    void erase(const Key& k) {
        std::lock_guard<std::mutex> lk(m_);
        auto it = map_.find(k);
        if (it == map_.end()) return;
        list_.erase(it->second.it);
        map_.erase(it);
    }

    void clear() {
        std::lock_guard<std::mutex> lk(m_);
        list_.clear();
        map_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lk(m_);
        return map_.size();
    }

private:
    struct Node { Value val; typename std::list<Key>::iterator it; };
    size_t capacity_;
    mutable std::mutex m_;
    std::list<Key> list_; // front = most recent
    std::unordered_map<Key, Node> map_;

    void touch(typename std::unordered_map<Key, Node>::iterator it) {
        list_.erase(it->second.it);
        list_.push_front(it->first);
        it->second.it = list_.begin();
    }
};
