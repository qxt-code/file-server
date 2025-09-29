#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "mysql_pool.h"
#include "types/file_metadata.h"

namespace db {

class FileRepository {
public:

    FileRepository(const FileRepository&) = delete;
    FileRepository& operator=(const FileRepository&) = delete;

    FileRepository(FileRepository&&);
    FileRepository& operator=(FileRepository&&);

    static FileRepository& getInstance() {
        static FileRepository instance{};
        return instance;
    }

    bool insertFile(const FileMetadata& file);

    
    std::optional<FileMetadata> getByHash(const std::string& hashCode);
    // New: fetch file metadata by numeric id
    std::optional<FileMetadata> getById(size_t file_id);

    bool increaseFile(const size_t file_id);

    /// @brief Reduce the reference count of a file. If the reference count reaches zero, the file can be deleted.
    ///
    /// @param fileId The ID of the file to reduce the reference count for.
    /// @return     True if the operation was successful, false otherwise.
    bool reduceFile(const size_t file_id);
    
    bool deleteFile(const size_t file_id);
    std::vector<FileMetadata> listFiles(const std::string& userId);

private:
    FileRepository();
    ~FileRepository();

    inline static MySQLPool* pool_;
};

}