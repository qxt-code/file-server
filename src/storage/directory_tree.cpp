#include "directory_tree.h"

#include <ranges>
#include <stack>

#include "db/user_file_repository.h"
#include "common/debug.h"


namespace storage {

DirectoryTree::DirectoryTree()
     : user_id_(-1) {
    root_ = new Trie();
    root_->file_id = 0; // Root directory ID
    current_ = root_;
}

DirectoryTree::DirectoryTree(int user_id)
     : user_id_(user_id) {
    root_ = new Trie();
    root_->file_id = 0; // Root directory ID
    current_ = root_;
}

DirectoryTree::~DirectoryTree() {
    delete root_;
    root_ = nullptr;
    current_ = nullptr;
}

DirectoryTree::DirectoryTree(DirectoryTree&& other) {
    root_ = other.root_;
    current_ = other.current_;
    user_id_ = other.user_id_;
    other.root_ = nullptr;
    other.current_ = nullptr;
    other.user_id_ = -1;
}

DirectoryTree& DirectoryTree::operator=(DirectoryTree&& other) {
    if (this != &other) {
        delete root_;
        root_ = other.root_;
        current_ = other.current_;
        user_id_ = other.user_id_;
        other.root_ = nullptr;
        other.current_ = nullptr;
        other.user_id_ = -1;
    }
    return *this;
}


DirectoryTree& DirectoryTree::operator/(const std::string& dir_name) {
    if (dir_name.empty() || dir_name == ".") {
        return *this;
    }
    if (dir_name == "..") {
        if (current_->parent != nullptr) {
            current_ = current_->parent;
        }
        return *this;
    }
    if (dir_name == "/") {
        current_ = root_;
        return *this;
    }
    if (current_->is_loaded == false) {
        loadDirectory();
    }
    
    auto it = current_->children.find(dir_name);
    if (it == current_->children.end()) {
        throw FileError(dir_name + ": No such file or directory");
    }
    if (it->second->user_file_meta_data == nullptr || it->second->user_file_meta_data->fileType != FileType::DIRECTORY) {
        throw FileError(dir_name + ": Not a directory");
    }
    current_ = current_->children[dir_name];
    if (current_->is_loaded == false) {
        // TODO load
    }
    return *this;
}

void DirectoryTree::cd(const std::string& path) {
    auto tokens = path | std::views::split('/') | std::views::transform([](auto&& rng) {
        return std::string(&*rng.begin(), std::ranges::distance(rng));
    });

    for (const auto& dir : tokens) {
        if (dir.empty() && !path.empty()) {
            *this / "/";
        } else {
            *this / dir;
        }
        loadDirectory();
    }
}

std::string DirectoryTree::ls() {
    std::string listing;
    loadDirectory();
    for (const auto& [name, child] : current_->children) {
        if (child->user_file_meta_data) {
            listing += name + " ";
        } else {
            error_cpp20("Corrupted directory entry: " + name);
        }
    }
    return listing;
}

std::string DirectoryTree::pwd() {
    std::string path;
    std::stack<std::string> dir_stack;
    loadDirectory();
    for (Trie* dir = current_; dir != nullptr; dir = dir->parent) {
        if (dir->user_file_meta_data) {
            dir_stack.push(dir->user_file_meta_data->fileName);
        }
    }
    while (!dir_stack.empty()) {
        path += "/" + dir_stack.top();
        dir_stack.pop();
    }
    return path.empty() ? "/" : path;
}

void DirectoryTree::mkdir(const std::string& dir_name) {
    loadDirectory();
    if (dir_name.empty() || dir_name == "." || dir_name == ".." || dir_name == "/") {
        error_cpp20("Invalid directory name: " + dir_name);
        throw FileError("Invalid directory name: " + dir_name);
    }
    if (current_->children.find(dir_name) != current_->children.end()) {
        error_cpp20("Directory already exists: " + dir_name);
        throw FileError("Directory already exists: " + dir_name);
    }
    if (user_id_ < 0) {
        error_cpp20("No user ID associated with DirectoryTree");
    }
    int user_id = user_id_;
    int parent_id = current_->file_id;

    UserFile newDir(dir_name, current_->user_file_meta_data ? current_->user_file_meta_data->filePath + "/" + dir_name : "/" + dir_name);
    newDir.userId = user_id;
    newDir.parentId = parent_id;
    newDir.fileType = FileType::DIRECTORY;

    bool success = db::UserFileRepository::getInstance().insertUserFile(newDir);
    if (!success) {
        error_cpp20("Failed to create directory in database: " + dir_name);
        throw FileError("Failed to create directory: " + dir_name);
    }

    Trie* child = new Trie();
    child->user_file_meta_data = new UserFile(newDir);
    child->parent = current_;
    current_->children[dir_name] = child;
}

void DirectoryTree::loadDirectory() {
    if (current_->is_loaded) return;
    if (user_id_ < 0) {
        error_cpp20("No user ID associated with DirectoryTree");
    }
    int user_id = user_id_;
    int parent_id = current_->file_id;

    auto files = db::UserFileRepository::getInstance().getFilesByParentID(user_id, parent_id);
    for (const auto& file : files) {
        if (file.parentId != current_->file_id) continue;
        if (current_->children.find(file.fileName) != current_->children.end()) {
            if (current_->children[file.fileName]->user_file_meta_data == nullptr) {
                current_->children[file.fileName]->user_file_meta_data = new UserFile(file);
            }
            continue; // already exists
        }
        Trie* child = new Trie();
        child->user_file_meta_data = new UserFile(file);
        child->parent = current_;
        child->file_id = file.id;
        current_->children[file.fileName] = child;
    }
    current_->is_loaded = true;
}

bool DirectoryTree::isFileExists(const std::string& file_name) {
    if (file_name.empty() || file_name == "." || file_name == ".." || file_name == "/") {
        error_cpp20("Invalid file name: " + file_name);
        throw FileError("Invalid file name: " + file_name);
        return false;
    }
    if (current_->children.find(file_name) != current_->children.end()) {
        return true;
    }
    return false;
}

bool DirectoryTree::createFile(const std::string& file_name, int file_id) {
    loadDirectory();
    if (file_name.empty() || file_name == "." || file_name == ".." || file_name == "/") {
        error_cpp20("Invalid file name: " + file_name);
        // throw FileError("Invalid file name: " + file_name);
        return false;
    }
    if (current_->children.find(file_name) != current_->children.end()) {
        error_cpp20("File already exists: " + file_name);
        // throw FileError("File already exists: " + file_name);
        return false;
    }
    if (user_id_ < 0) {
        error_cpp20("No user ID associated with DirectoryTree");
    }
    int user_id = user_id_;
    int parent_id = current_->file_id;

    UserFile newFile(file_name, current_->user_file_meta_data ? current_->user_file_meta_data->filePath + "/" + file_name : "/" + file_name);
    newFile.userId = user_id;
    newFile.parentId = parent_id;
    newFile.fileId = file_id;
    newFile.fileType = FileType::FILE;

    bool success = db::UserFileRepository::getInstance().insertUserFile(newFile);
    if (!success) {
        error_cpp20("Failed to create file in database: " + file_name);
        throw FileError("Failed to create file: " + file_name);
    }

    Trie* child = new Trie();
    child->user_file_meta_data = new UserFile(newFile);
    child->parent = current_;
    child->file_id = newFile.id;
    current_->children[file_name] = child;
    return true;
}

DirectoryTree::DeleteResult DirectoryTree::deleteFile(const std::string& file_name) {
    DeleteResult result;
    loadDirectory();
    auto it = current_->children.find(file_name);
    if (it == current_->children.end()) {
        return result;
    }
    Trie* node = it->second;
    if (!node || !node->user_file_meta_data) {
        return result;
    }
    result.type = node->user_file_meta_data->fileType;
    result.user_file_row_id = node->user_file_meta_data->id;
    result.file_id = static_cast<int>(node->user_file_meta_data->fileId);

    if (result.type == FileType::DIRECTORY) {
        if (!node->children.empty()) {
            error_cpp20("Directory not empty: " + file_name);
            return result;
        }
    }
    delete node;
    current_->children.erase(it);
    result.success = true;
    return result;
}

bool DirectoryTree::isDirectoryEmpty(const std::string& dir_name) {
    loadDirectory();
    auto it = current_->children.find(dir_name);
    if (it == current_->children.end()) return false;
    Trie* node = it->second;
    if (!node || !node->user_file_meta_data) return false;
    if (node->user_file_meta_data->fileType != FileType::DIRECTORY) return false;

    if (!node->is_loaded) {

        Trie* prev = current_;
        current_ = node;
        loadDirectory();
        current_ = prev;
    }
    return node->children.empty();
}


} // namespace storage