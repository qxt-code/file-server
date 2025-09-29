#pragma once

#include <unordered_map>
#include <string>
#include <mutex>
#include <optional>
#include "types/file_metadata.h"
#include <filesystem>

namespace storage {

namespace fs = std::filesystem;

class GlobalOpenTable {
public:
    static void init(const std::string& storage_root);

    static GlobalOpenTable& getInstance() {
        static GlobalOpenTable instance{};
        return instance;
    }

    GlobalOpenTable(const GlobalOpenTable&) = delete;
    GlobalOpenTable& operator=(const GlobalOpenTable&) = delete;

    std::optional<int> openFile(const std::string& file_name);
    void closeFile(const std::string& file_name);
    bool isFileOpen(const std::string& file_name) const;

private:
    GlobalOpenTable();
    ~GlobalOpenTable();

    struct FileEntry {
        int refCount;
        int fd;
    };

    std::unordered_map<std::string, FileEntry> open_files_;
    mutable std::mutex mutex_;

    inline static fs::path storage_root_;
    inline static bool is_initialized_{false};
};

} // namespace storage