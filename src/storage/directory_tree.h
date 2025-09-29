#pragma once

#include <map>

#include "types/user_file.h"
#include "storage_error.h"
#include "types/context.h"


namespace storage {

class DirectoryTree {
public:
    DirectoryTree();
    DirectoryTree(int user_id);
    ~DirectoryTree();

    void setUserId(int user_id) { user_id_ = user_id; }
    
    DirectoryTree(const DirectoryTree&) = delete;
    DirectoryTree& operator=(const DirectoryTree&) = delete;

    DirectoryTree(DirectoryTree&&);
    DirectoryTree& operator=(DirectoryTree&&);

    DirectoryTree& operator/(const std::string& dir_name);

    void cd(const std::string& path);
    std::string ls();
    std::string pwd();
    void mkdir(const std::string& dir_name);

    bool isFileExists(const std::string& file_name);
    bool createFile(const std::string& file_name, int file_id);
    struct DeleteResult {
        bool success{false};
        int file_id{-1}; // file repository id (fileId)
        size_t user_file_row_id{0}; // user_files.id
        FileType type{FileType::FILE};
    };
    DeleteResult deleteFile(const std::string& file_name);
    bool isDirectoryEmpty(const std::string& dir_name);

private:
    void loadDirectory();

    struct Trie {
        bool is_loaded{false};
        int file_id{-1}; // File ID in the database
        UserFile* user_file_meta_data{nullptr}; // Pointer to associated UserFile metadata
        Trie* parent{nullptr};
        std::map<std::string, Trie*> children;
        Trie() {}
        ~Trie() {
            for (auto& [name, child] : children) {
                delete child;
            }
            delete user_file_meta_data;
        }
    };
    Trie* root_;
    Trie* current_;
    int user_id_;

};

} // namespace storage