#include "file_manager.h"

#include "common/debug.h"
#include "db/file_repository.h"
#include "cache/file_meta_cache.h"
#include "db/user_file_repository.h"

namespace storage {

void FileManager::setUser(int user_id) {
    if (directory_tree_map_.find(user_id) == directory_tree_map_.end()) {
        directory_tree_map_[user_id] = DirectoryTree(user_id);
    }
}

void FileManager::cd(int user_id, const std::string& path) {
    directory_tree_map_[user_id].cd(path);
}

std::string FileManager::ls(int user_id) {
    return directory_tree_map_[user_id].ls();
}

std::string FileManager::pwd(int user_id) {
    return directory_tree_map_[user_id].pwd();
}

void FileManager::mkdir(int user_id,const std::string& dir_name) {
    directory_tree_map_[user_id].mkdir(dir_name);
}

bool FileManager::isFileExists(int user_id, const std::string& file_name) {
    return directory_tree_map_[user_id].isFileExists(file_name);
}

bool FileManager::deleteFile(int user_id, const std::string& file_name) {
    DirectoryTree& directory_tree = directory_tree_map_[user_id];
    if (!directory_tree.isFileExists(file_name)) {
        error_cpp20("Delete failed, file not exists: " + file_name);
        return false;
    }
    auto delRes = directory_tree.deleteFile(file_name);
    if (!delRes.success) {
        return false;
    }

    if (delRes.user_file_row_id > 0) {
        db::UserFileRepository::getInstance().deleteUserFile(delRes.user_file_row_id);
    }

    if (delRes.type == FileType::FILE && delRes.file_id >= 0) {
        // Need metadata for possible invalidation or refcount logic; fetch via cache (will populate if not present)
        auto meta_before = FileMetaCache::instance().getById(delRes.file_id);
        db::FileRepository::getInstance().reduceFile(delRes.file_id);
        // Refcount change may lead to deletion; safest is to invalidate id (hash invalidation will occur when next fetched if necessary)
        if (meta_before) {
            FileMetaCache::instance().invalidateId(delRes.file_id);
            FileMetaCache::instance().invalidateHash(meta_before->hashCode);
        }
        // TODO: physical delete when global refcount reaches zero (will require checking repository state)
    }
    return true;
}

bool FileManager::removeDirectory(int user_id, const std::string& dir_name) {
    DirectoryTree& directory_tree = directory_tree_map_[user_id];
    if (!directory_tree.isFileExists(dir_name)) {
        error_cpp20("rmdir failed, dir not exists: " + dir_name);
        return false;
    }
    auto delRes = directory_tree.deleteFile(dir_name);
    if (!delRes.success || delRes.type != FileType::DIRECTORY) {
        return false;
    }
    if (delRes.user_file_row_id > 0) {
        db::UserFileRepository::getInstance().deleteUserFile(delRes.user_file_row_id);
    }
    return true;
}

void FileManager::createFile(int user_id, const std::string& file_name, const std::string file_hash, int file_size) {
    DirectoryTree& directory_tree = directory_tree_map_[user_id];
    if (directory_tree.isFileExists(file_name)) {
        error_cpp20("File already exists: " + file_name);
        return;
    }

    auto meta_data_opt = FileMetaCache::instance().getByHash(file_hash);
    FileMetadata meta_data("", 0);
    if (!meta_data_opt.has_value()) {
        db::FileRepository::getInstance().insertFile(FileMetadata(file_hash, file_size));
        meta_data_opt = FileMetaCache::instance().getByHash(file_hash); // will miss; underlying repo fetch; consider explicit insert
        meta_data = meta_data_opt.value();
        // Ensure cache population if repository fetch path bypassed adapter logic
        FileMetaCache::instance().insert(meta_data);
    } else {
        db::FileRepository::getInstance().increaseFile(meta_data_opt->id);
        meta_data = meta_data_opt.value();
        // After refcount increase we can refresh cache entry
        FileMetaCache::instance().invalidateId(meta_data.id);
        FileMetaCache::instance().invalidateHash(meta_data.hashCode);
        FileMetaCache::instance().insert(meta_data);
    }
    file_metadata_map_[meta_data.id] = meta_data;
    directory_tree.createFile(file_name, meta_data.id);
}

int FileManager::openFile(int user_id, const std::string& file_name, const std::string& file_hash) {
    UserOpenTable& user_open_table = user_open_table_map_[user_id];
    DirectoryTree& directory_tree = directory_tree_map_[user_id];

    if (user_open_table.isFileOpen(file_hash)) {
        return user_open_table.getFD(file_hash);
    }

    auto fd_opt = user_open_table.openFile(file_hash);
    if (!fd_opt.has_value()) {
        return -1; // Failed to open file
    }

    return fd_opt.value();
}

UserFileHandle::Ptr FileManager::getFileHandle(int user_id, int user_fd) {
    UserOpenTable& user_open_table = user_open_table_map_[user_id];
    return user_open_table.getFileHandle(user_fd);
}

} // namespace storage