#include "mysql_pool.h"
#include "user_file_repository.h"
#include "common/debug.h"
#include <optional>
#include <sstream>

    // size_t id;                  // Unique identifier for the file
    // size_t userId;             // Owner user ID
    // size_t parentId;            // Identifier of the parent directory
    // size_t fileId;               // File identifier
    // std::string fileName;       // Name of the file
    // std::string filePath;       // Path to the file
    // FileType fileType;       // Type of the file (FILE, DIRECTORY, SYMLINK)
    // std::chrono::system_clock::time_point createdTime; // Creation time
    // std::chrono::system_clock::time_point modifiedTime; // Last modified time
    // bool isDeleted;          // Indicates if the file is deleted


namespace db {

const std::string user_file_fields = 
    "id, user_id, parent_id, file_id, file_name, file_path, file_type, created_at, updated_at, is_deleted";
const std::string user_file_fields_no_id = 
    "user_id, parent_id, file_id, file_name, file_path, file_type, created_at, updated_at, is_deleted";

UserFile row2user_file(MYSQL_ROW row) {
    UserFile file("", "");
    file.id = std::stoul(row[0]);
    file.userId = std::stoul(row[1]);
    file.parentId = std::stoul(row[2]);
    file.fileId = std::stoul(row[3]);
    file.fileName = row[4] ? row[4] : "";
    file.filePath = row[5] ? row[5] : "";
    file.fileType = static_cast<FileType>(std::stoi(row[6]));
    file.createdTime = std::chrono::system_clock::now();
    file.modifiedTime = std::chrono::system_clock::now();
    file.isDeleted = (row[8] && std::string(row[8]) == "1");
    return file;
}

std::string get_user_file_insert_query(const UserFile& userFile) {
    return "INSERT INTO user_files (" + user_file_fields_no_id + ") VALUES (" +
           std::to_string(userFile.userId) + ", " +
           std::to_string(userFile.parentId) + ", " +
           std::to_string(userFile.fileId) + ", '" +
           userFile.fileName + "', '" +
           userFile.filePath + "', " +
           std::to_string(static_cast<int>(userFile.fileType)) + ", NOW(), NOW(), " +
           (userFile.isDeleted ? "1" : "0") + ")";
}

std::string get_user_file_select_query(const std::string condition) {
    return "SELECT " + user_file_fields + " FROM user_files WHERE " + condition;
}

UserFileRepository::UserFileRepository() {

    pool_ = &MySQLPool::getInstance();
    if (!is_initialized_) {
        is_initialized_ = true;
    } 

}

UserFileRepository::~UserFileRepository() {
    pool_ = nullptr;
    is_initialized_ = false;
}

bool UserFileRepository::insertUserFile(const UserFile& userFile) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = get_user_file_insert_query(userFile);
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL insert error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}

bool UserFileRepository::deleteUserFile(const size_t fileId) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = "UPDATE user_files SET is_deleted = 1 WHERE id = " + std::to_string(fileId);
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL delelte error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}

std::vector<UserFile> UserFileRepository::getFilesByParentID(int userId, int parentId) {
    std::vector<UserFile> files;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return files;

    std::string query = get_user_file_select_query(
        "user_id = '" + std::to_string(userId) + "' AND parent_id = " + std::to_string(parentId) + " AND is_deleted = 0"
    );

    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL query error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return files;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return files;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {

        files.push_back(row2user_file(row));
    }

    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return files;

}

std::optional<UserFile> UserFileRepository::getFileByParentAndName(int userId, int parentId, const std::string& name) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return std::nullopt;
    std::string condition = "user_id = '" + std::to_string(userId) + "' AND parent_id = " + std::to_string(parentId) +
        " AND file_name = '" + name + "' AND is_deleted = 0";
    std::string query = get_user_file_select_query(condition);
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL query error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    UserFile uf = row2user_file(row);
    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return uf;
}

std::optional<UserFile> UserFileRepository::getFileByPath(int userId, const std::string& virtualPath) {
    // naive implementation: split by '/' and iteratively descend using parentId
    if (virtualPath.empty() || virtualPath == "/") return std::nullopt; // root directory not a file
    std::string path = virtualPath;
    if (path[0] == '/') path.erase(0,1);
    std::stringstream ss(path);
    std::string segment;
    int currentParent = 0; // assume 0 is root parent_id
    std::optional<UserFile> current;
    while (std::getline(ss, segment, '/')) {
        if (segment.empty()) continue;
        current = getFileByParentAndName(userId, currentParent, segment);
        if (!current) return std::nullopt;
        currentParent = static_cast<int>(current->id); // use user_files.id as next parent for directories
    }
    return current;
}

} // namespace db