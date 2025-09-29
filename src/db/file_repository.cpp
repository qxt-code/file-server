#include "file_repository.h"

#include <string>
#include <vector>


#include <mysql/mysql.h>

#include "mysql_pool.h"
#include "common/debug.h"
#include "db_error.h"

// struct FileMetadata {
//     size_t id;                  // Unique identifier for the file
//     std::string hashCode;    // Hash code of the file
//     size_t fileSize;            // Size of the file in bytes
//     size_t receivedBytes;      // Number of bytes received (for transfers)
//     size_t sentBytes;          // Number of bytes sent (for transfers)
//     std::chrono::system_clock::time_point createdTime; // Creation time
//     std::chrono::system_clock::time_point modifiedTime; // Last modified time
//     size_t refCount;           // Reference count for the file

//     FileMetadata(std::string hash, size_t size)
//         : hashCode(hash), fileSize(size), refCount(1) {
//         createdTime = std::chrono::system_clock::now();
//         modifiedTime = createdTime;
//     }

//     void updateModifiedTime() {
//         modifiedTime = std::chrono::system_clock::now();
//     }
// };

// CREATE TABLE files (
//     id INT AUTO_INCREMENT PRIMARY KEY,
//     hash_code VARCHAR(255) NOT NULL,
//     file_size BIGINT NOT NULL,
//     received_bytes BIGINT NOT NULL,
//     sent_bytes BIGINT NOT NULL,
//     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
//     updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
//     ref_count INT DEFAULT 1
// );


namespace db {

const std::string file_fields = 
    "id, hash_code, file_size, received_bytes, sent_bytes, created_at, updated_at, ref_count";
const std::string file_fields_no_id = 
    "hash_code, file_size, received_bytes, sent_bytes, created_at, updated_at, ref_count";

FileMetadata row2file_metadata(MYSQL_ROW row) {
    FileMetadata file("", 0);
    file.id = std::stoul(row[0]);
    file.hashCode = std::string(row[1]);
    file.fileSize = std::stoul(row[2]);
    file.receivedBytes = std::stoul(row[3]);
    file.sentBytes = std::stoul(row[4]);
    file.createdTime = std::chrono::system_clock::now();
    file.modifiedTime = std::chrono::system_clock::now();
    file.refCount = std::stoul(row[5]);
    return file;
}

std::string get_file_metadata_insert_query(const FileMetadata& file_metadata) {
    return "INSERT INTO files ( " + file_fields_no_id + " ) VALUES ( '" +
           file_metadata.hashCode + "' , " +
           std::to_string(file_metadata.fileSize) + ", " +
           std::to_string(file_metadata.receivedBytes) + ", " +
           std::to_string(file_metadata.sentBytes) + ", NOW(), NOW(), " +
           std::to_string(file_metadata.refCount) + ")";
}

std::string get_user_file_select_query(const std::string& condition) {
    return "SELECT " + file_fields + " FROM files WHERE " + condition;
}

FileRepository::FileRepository() {
    pool_ = &MySQLPool::getInstance();
}

FileRepository::~FileRepository() {
    pool_ = nullptr;
}

FileRepository::FileRepository(FileRepository&& other) {
    pool_ = other.pool_;
    other.pool_ = nullptr;
}

FileRepository& FileRepository::operator=(FileRepository&& other) {
    if (this != &other) {
        pool_ = other.pool_;
        other.pool_ = nullptr;
    }
    return *this;
}

bool FileRepository::insertFile(const FileMetadata& file) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = get_file_metadata_insert_query(file);
  
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        error_cpp20("MySQL insert failed: " + std::string(mysql_error(conn)));
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}

bool FileRepository::increaseFile(const size_t file_id) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = "UPDATE files SET refcount = refcount + 1 WHERE id = '" + std::to_string(file_id) + "'";
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}


bool FileRepository::reduceFile(const size_t file_id) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = "UPDATE files SET refcount = refcount - 1 WHERE id = '" + std::to_string(file_id) + "'";
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}


bool FileRepository::deleteFile(const size_t file_id) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = "DELETE FROM files WHERE id = '" + std::to_string(file_id) + "'";
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        return false;
    }

    pool_->releaseConnection(conn);
    return true;
}

std::optional<FileMetadata> FileRepository::getByHash(const std::string& hashCode) {
    // Fast path via cache (read-through). To avoid recursive dependency, cache header included elsewhere.
    // didn't include cache header here directly to keep layering minimal; external callers may prefer using adapter.
    MYSQL* conn = pool_->getConnection();
    if (!conn) {
        error_cpp20("MySQL query failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL query failed: " + std::string(mysql_error(conn)));
    }

    std::string query = get_user_file_select_query("hash_code = '" + hashCode + "'");
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        error_cpp20("MySQL query failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL query failed: " + std::string(mysql_error(conn)));
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool_->releaseConnection(conn);
        error_cpp20("MySQL store result failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL store result failed: " + std::string(mysql_error(conn)));
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool_->releaseConnection(conn);
        return std::nullopt;
    }

    FileMetadata file = row2file_metadata(row);

    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return file;
}

std::optional<FileMetadata> FileRepository::getById(size_t file_id) {
    MYSQL* conn = pool_->getConnection();
    if (!conn) {
        error_cpp20("MySQL query failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL query failed: " + std::string(mysql_error(conn)));
    }

    std::string query = get_user_file_select_query("id = '" + std::to_string(file_id) + "'");
    if (mysql_query(conn, query.c_str())) {
        pool_->releaseConnection(conn);
        error_cpp20("MySQL query failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL query failed: " + std::string(mysql_error(conn)));
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool_->releaseConnection(conn);
        error_cpp20("MySQL store result failed: " + std::string(mysql_error(conn)));
        throw DBError("MySQL store result failed: " + std::string(mysql_error(conn)));
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool_->releaseConnection(conn);
        return std::nullopt;
    }

    FileMetadata file = row2file_metadata(row);

    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return file;
}


}