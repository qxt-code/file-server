#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <memory>

#include "lockfreequeue/array_mpmc_queue.hpp"
#include "lockfreequeue/adaptive_blocking_queue.hpp"


namespace db {

using lf::ArrayMPMCQueue;
using lf::AdaptiveBlockingQueue;

struct MySQLConfig {
    std::string host;
    std::string user;
    std::string password;
    std::string db;
    unsigned int maxConnections;
};

class MySQLPool {
public:
    MySQLPool(const MySQLPool&) = delete;
    MySQLPool& operator=(const MySQLPool&) = delete;

    static void init(const MySQLConfig& config);
    static MySQLPool& getInstance() {
        static MySQLPool instance{};
        return instance;
    }
    void destroy();
    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);

private:
    MySQLPool();
    ~MySQLPool();

    inline static MySQLConfig config_{};
    inline static bool is_initialized_{false};
    
    lf::AdaptiveBlockingQueue<ArrayMPMCQueue<MYSQL*>> connections_;
    
};

} // namespace db