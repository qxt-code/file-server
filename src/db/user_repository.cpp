
#include "user_repository.h"
#include <mysql/mysql.h>
#include <sstream>

// CREATE TABLE users (
//     id INT AUTO_INCREMENT PRIMARY KEY,
//     username VARCHAR(255) NOT NULL UNIQUE,
//     password_hash VARCHAR(255) NOT NULL,
//     salt VARCHAR(255) NOT NULL,
//     email VARCHAR(255) NOT NULL UNIQUE,
//     created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
//     updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
// );

namespace db {

const std::string user_fields = 
    "id, username, password_hash, salt, email, created_at, updated_at";
const std::string user_fields_no_id = 
    "username, password_hash, salt, email, created_at, updated_at";


static User row2user(MYSQL_ROW row) {
    int id = row[0] ? std::stoi(row[0]) : 0;
    std::string username = row[1] ? row[1] : "";
    std::string password_hash = row[2] ? row[2] : "";
    std::string salt = row[3] ? row[3] : "";
    std::string email = row[4] ? row[4] : "";
    // created_at row[5], updated_at row[6] ignored for now
    User u; u.id = id; u.username = username; u.password_hash = password_hash; u.salt = salt; u.email = email; return u;
}

std::string get_user_insert_query(const User& user) {
    return "INSERT INTO users (" + user_fields_no_id + ") VALUES ('" +
           user.username + "', '" +
           user.password_hash + "', '" +
           user.salt + "', '" +
           user.email + "', NOW(), NOW())";
}
std::string get_user_select_query(const std::string& condition) {
    return "SELECT " + user_fields + " FROM users WHERE " + condition;
}

UserRepository::UserRepository() {
    pool_ = &MySQLPool::getInstance();
    if (!is_initialized_) {
        is_initialized_ = true;
    } 
}

bool UserRepository::createUser(const User& user) {
    if (!pool_) return false;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::string query = get_user_insert_query(user);
    log_cpp20("[UserRepository] createUser query=" + query);

    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL insert user error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }
    pool_->releaseConnection(conn);
    return true;
}

bool UserRepository::deleteUser(int userId) {
    if (!pool_) return false;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;
    std::string query = "DELETE FROM users WHERE id = " + std::to_string(userId);
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL delete user error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }
    pool_->releaseConnection(conn);
    return mysql_affected_rows(conn) > 0;
}

bool UserRepository::updateUser(const User& user) {
    if (!pool_) return false;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;

    std::ostringstream oss;
    oss << "UPDATE users SET username='" << user.username
        << "', password_hash='" << user.password_hash
        << "', email='" << user.email
        << "' WHERE id=" << user.id;
    std::string query = oss.str();

    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL update user error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }
    bool ok = mysql_affected_rows(conn) > 0;
    pool_->releaseConnection(conn);
    return ok;
}

std::optional<User> UserRepository::getUserById(int userId) {
    if (!pool_) return std::nullopt;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return std::nullopt;

    std::string query = get_user_select_query("id = " + std::to_string(userId) + " LIMIT 1");
   
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL select user error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store result error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::optional<User> result;
    if (row) result = row2user(row);
    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return result;
}

std::optional<User> UserRepository::getUserByName(const std::string& username) {
    if (!pool_) return std::nullopt;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return std::nullopt;

    std::string query = get_user_select_query("username = '" + username + "' LIMIT 1");
    
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL select user by name error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store result error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return std::nullopt;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    std::optional<User> result;
    if (row) result = row2user(row);
    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return result;
}

std::vector<User> UserRepository::getAllUsers() {
    std::vector<User> users;
    if (!pool_) return users;
    MYSQL* conn = pool_->getConnection();
    if (!conn) return users;
    std::string query = "SELECT " + user_fields + " FROM users";
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL select all users error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return users;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store result error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return users;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != nullptr) {
        users.push_back(row2user(row));
    }
    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return users;
}

bool UserRepository::usernameExists(const std::string& username) {
    if (!pool_) return false; // treat as not exists on failure context
    MYSQL* conn = pool_->getConnection();
    if (!conn) return false;
    std::string query = "SELECT COUNT(*) FROM users WHERE username='" + username + "'";
    log_cpp20("[UserRepository] usernameExists query=" + query);
    if (mysql_query(conn, query.c_str())) {
        error_cpp20("MySQL username exists error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        error_cpp20("MySQL store result error: " + std::string(mysql_error(conn)));
        pool_->releaseConnection(conn);
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = false;
    if (row && row[0]) {
        exists = std::stoll(row[0]) > 0;
    }
    mysql_free_result(res);
    pool_->releaseConnection(conn);
    return exists;
}

} // namespace db