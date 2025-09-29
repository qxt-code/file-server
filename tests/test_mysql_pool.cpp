#include "gtest/gtest.h"
#include "db/mysql_pool.h"
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>

const std::string DB_HOST = "127.0.0.1";
const std::string DB_USER = "root";
const std::string DB_PASS = "123456";
const std::string DB_NAME = "test";
const unsigned int POOL_SIZE = 10;

class MySQLPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        MYSQL* temp_conn = mysql_init(nullptr);
        ASSERT_NE(temp_conn, nullptr);
        ASSERT_NE(
            mysql_real_connect(temp_conn, DB_HOST.c_str(), DB_USER.c_str(), DB_PASS.c_str(), nullptr, 0, nullptr, 0),
            nullptr
        ) << "cannot connect to MySQL server: " << mysql_error(temp_conn);
        
        mysql_query(temp_conn, ("CREATE DATABASE IF NOT EXISTS " + DB_NAME).c_str());
        mysql_close(temp_conn);
    }
    
    void TearDown() override {
    }
};

TEST_F(MySQLPoolTest, ConstructionSucceedsWithValidCredentials) {
    std::unique_ptr<MySQLPool> pool = nullptr;
    ASSERT_NO_THROW({
        pool = std::make_unique<MySQLPool>(DB_HOST, DB_USER, DB_PASS, DB_NAME, POOL_SIZE);
    });
    ASSERT_NE(pool, nullptr);
}

TEST_F(MySQLPoolTest, ConstructionThrowsWithInvalidCredentials) {
    ASSERT_THROW({
        MySQLPool pool(DB_HOST, "invalid_user", "invalid_pass", DB_NAME, POOL_SIZE);
    }, std::runtime_error);
}

TEST_F(MySQLPoolTest, SingleConnectionAcquireAndRelease) {
    MySQLPool pool(DB_HOST, DB_USER, DB_PASS, DB_NAME, POOL_SIZE);
    MYSQL* conn = nullptr;

    ASSERT_NO_THROW({
        conn = pool.getConnection();
    });
    ASSERT_NE(conn, nullptr);
    

    ASSERT_EQ(mysql_query(conn, "SELECT 1"), 0) << mysql_error(conn);


    pool.releaseConnection(conn);
}


TEST_F(MySQLPoolTest, AcquireAllConnections) {
    const unsigned int size = 5;
    MySQLPool pool(DB_HOST, DB_USER, DB_PASS, DB_NAME, size);
    std::vector<MYSQL*> connections;

    for (unsigned int i = 0; i < size; ++i) {
        MYSQL* conn = nullptr;
        ASSERT_NO_THROW({
            conn = pool.getConnection();
        });
        ASSERT_NE(conn, nullptr);
        connections.push_back(conn);
    }


    ASSERT_EQ(connections.size(), size);


    for (auto conn : connections) {
        pool.releaseConnection(conn);
    }
}


TEST_F(MySQLPoolTest, BlocksWhenPoolIsEmptyAndUnblocksOnRelease) {
    const unsigned int size = 1;
    MySQLPool pool(DB_HOST, DB_USER, DB_PASS, DB_NAME, size);


    MYSQL* first_conn = pool.getConnection();
    ASSERT_NE(first_conn, nullptr);

    std::atomic<bool> thread_started = false;
    std::atomic<bool> connection_acquired = false;


    std::thread t1([&]() {
        thread_started = true;

        MYSQL* second_conn = pool.getConnection();
        if (second_conn != nullptr) {
            connection_acquired = true;

            pool.releaseConnection(second_conn);
        }
    });


    while (!thread_started) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    ASSERT_FALSE(connection_acquired);
    

    std::cout << "Releasing connection to unblock the thread..." << std::endl;
    pool.releaseConnection(first_conn);


    t1.join();
    

    ASSERT_TRUE(connection_acquired);
}



TEST_F(MySQLPoolTest, HeavyContention) {
    const int num_threads = 50;
    const int ops_per_thread = 100;
    MySQLPool pool(DB_HOST, DB_USER, DB_PASS, DB_NAME, POOL_SIZE);
    
    std::vector<std::thread> threads;
    std::atomic<int> successful_ops = 0;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ops_per_thread; ++j) {
                MYSQL* conn = nullptr;
                // try {
                    conn = pool.getConnection();
                    if (conn) {

                        if (mysql_query(conn, "SELECT 1") == 0) {
                            successful_ops++;
                            MYSQL_RES* res = mysql_store_result(conn);
                            if (res) {
                                mysql_free_result(res);
                            }
                        }
                        pool.releaseConnection(conn);
                    }
                // } catch (const std::exception& e) {

                //     FAIL() << "Exception caught in thread: " << e.what();
                // }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }


    EXPECT_EQ(successful_ops, num_threads * ops_per_thread);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}