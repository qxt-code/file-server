#include "mysql_pool.h"
extern "C" {
    #include <mysql/mysql.h>
}
#include <stdexcept>
#include <vector>
#include <mutex>

#include "common/debug.h"

#if defined(__i386__) || defined(__x86_64__)
#include <immintrin.h>
#define PAUSE_INSTRUCTION() _mm_pause()
#else
// on non-x86 architectures, define as no-op
#define PAUSE_INSTRUCTION()
#endif

namespace db {

void MySQLPool::init(const MySQLConfig& config) {
    config_ = config;
    is_initialized_ = true;
}


MySQLPool::MySQLPool() : connections_(config_.maxConnections) {
    if (!is_initialized_) {
        RUNTIME_ERROR("MySQLPool not initialized. Call MySQLPool::init() first.");
        return;
    }

    for (unsigned int i = 0; i < config_.maxConnections; ++i) {
        MYSQL* conn = mysql_init(nullptr);
        if (conn == nullptr || 
                mysql_real_connect(conn, config_.host.c_str(), 
                                    config_.user.c_str(), 
                                    config_.password.c_str(), 
                                    config_.db.c_str(), 0, nullptr, 0) == nullptr) {
            RUNTIME_ERROR("MySQL connection failed: %s", mysql_error(conn));
        }
        connections_.push(conn);
    }
}

MySQLPool::~MySQLPool() {
    destroy();
}

void MySQLPool::destroy() {
    MYSQL* conn;
    while (!connections_.empty()) {
        if (connections_.pop(conn))
            mysql_close(conn);
    }
}

MYSQL* MySQLPool::getConnection() {
    MYSQL* conn = nullptr;

    while (!connections_.pop(conn)) {}
    return conn;
}

void MySQLPool::releaseConnection(MYSQL* conn) {
    if (conn == nullptr) {
        return;
    }
    connections_.push(conn);
}

} // namespace db