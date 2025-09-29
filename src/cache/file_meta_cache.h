#pragma once
#include <optional>
#include <string>
#include <cstddef>
#include <shared_mutex>
#include <atomic>
#include "cache/lru_cache.h"
#include "db/file_repository.h"


class FileMetaCache {
public:
    static FileMetaCache& instance() {
        static FileMetaCache inst;
        return inst;
    }

    // Configure capacity (number of distinct file metas) - call early at startup.
    void configure(size_t capacity) {
        std::unique_lock<std::shared_mutex> lk(mu_);
        id_cache_ = std::make_unique<IdCache>(capacity);
        hash_cache_ = std::make_unique<HashCache>(capacity);
    }

    std::optional<FileMetadata> getById(size_t id) {
        ensureInit();
        if (auto v = id_cache_->get(id)) {
            ++hits_id_;
            return v;
        }
        ++misses_id_;
        auto meta = db::FileRepository::getInstance().getById(id);
        if (meta)
            insertUnlocked(*meta);
        return meta;
    }

    std::optional<FileMetadata> getByHash(const std::string& hash) {
        ensureInit();
        if (auto v = hash_cache_->get(hash)) {
            ++hits_hash_;
            return v;
        }
        ++misses_hash_;
        auto meta = db::FileRepository::getInstance().getByHash(hash);
        if (meta)
            insertUnlocked(*meta);
        return meta;
    }

    void invalidateId(size_t id) {
        ensureInit();
        id_cache_->erase(id);
    }

    void invalidateHash(const std::string& hash) {
        ensureInit();
        hash_cache_->erase(hash);
    }

    void insert(const FileMetadata& meta) {
        ensureInit();
        insertUnlocked(meta);
    }

    struct Stats {
        uint64_t hits_id;
        uint64_t misses_id;
        uint64_t hits_hash;
        uint64_t misses_hash;
    };
    Stats stats() const {
        return {
            hits_id_.load(),
            misses_id_.load(),
            hits_hash_.load(),
            misses_hash_.load()
        };
    }

private:
    using IdCache = LRUCache<size_t, FileMetadata>;
    using HashCache = LRUCache<std::string, FileMetadata>;

    std::unique_ptr<IdCache> id_cache_;
    std::unique_ptr<HashCache> hash_cache_;
    std::shared_mutex mu_;
    std::atomic<uint64_t> hits_id_{0}, misses_id_{0}, hits_hash_{0}, misses_hash_{0};

    void ensureInit() {
        if (!id_cache_) configure(defaultCapacity());
    }

    static size_t defaultCapacity() { return 4096; }

    void insertUnlocked(const FileMetadata& meta) {
        id_cache_->put(meta.id, meta);
        hash_cache_->put(meta.hashCode, meta);
    }
};
