cmake_minimum_required(VERSION 3.15)

# Set the project name
project(file_server_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the required packages
find_package(GTest REQUIRED)

enable_testing()

# Add the test executable
add_executable(file_server_tests
    # test_lockfreequeue_adaptive.cpp
    test_mysql_pool.cpp
    ../src/db/mysql_pool.cpp
    # other tests can be re-added when dependencies fixed
)

# Link the test executable with GTest and pthread
target_link_libraries(file_server_tests 
    mysqlclient
    GTest::gtest
    GTest::gtest_main
    pthread
    lockfreequeue
)

include(GoogleTest)
gtest_discover_tests(file_server_tests)

# Benchmark executable (not part of gtest discovery)
add_executable(lockfreequeue_bench lockfreequeue_bench.cpp)
target_link_libraries(lockfreequeue_bench pthread lockfreequeue)

add_executable(stress_lockfreequeue_adaptive stress_lockfreequeue_adaptive.cpp)
target_link_libraries(stress_lockfreequeue_adaptive pthread lockfreequeue)