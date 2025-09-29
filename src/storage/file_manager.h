#pragma once

#include <string>
#include <unordered_map>


#include "types/file_metadata.h"
#include "types/context.h"
#include "directory_tree.h"
#include "user_open_table.h"
#include "user_file_handle.h"

namespace storage {
    
class FileManager {
public:

    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    FileManager(FileManager&&) = default;
    FileManager& operator=(FileManager&&) = default;

    inline static FileManager& getInstance() {
        static FileManager instance{};
        return instance;
    }

    void setUser(int user_id);


    void cd(int user_id, const std::string& path);
    std::string ls(int user_id);
    std::string pwd(int user_id);
    void mkdir(int user_id, const std::string& dir_name);
    bool deleteFile(int user_id, const std::string& file_name);
    bool removeDirectory(int user_id, const std::string& dir_name);

    bool isFileExists(int user_id, const std::string& file_name);
    void createFile(int user_id, const std::string& file_name, const std::string file_hash, int file_size);
    int openFile(int user_id, const std::string& file_name, const std::string& file_hash);
    UserFileHandle::Ptr getFileHandle(int user_id, int user_fd);
    void closeFile(int user_id, int user_fd);


    bool uploadFile(const std::string& userId, const std::string& filePath);
    bool downloadFile(const std::string& userId, const std::string& fileId);
    bool deleteFile(const std::string& userId, const std::string& fileId);
    std::unordered_map<std::string, FileMetadata> listUserFiles(const std::string& userId);

private:
    FileManager() = default;
    ~FileManager() = default;

    std::unordered_map<int, FileMetadata> file_metadata_map_; // Maps file hash to metadata
    std::unordered_map<int, DirectoryTree> directory_tree_map_;
    std::unordered_map<int, UserOpenTable> user_open_table_map_;

};

} // namespace storage