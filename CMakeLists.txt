cmake_minimum_required(VERSION 3.15)

if (POLICY CMP0048)
    cmake_policy(SET CMP0048 NEW)
endif(POLICY CMP0048)

project(FTP VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(components)

include_directories(include)
include_directories(src)

include(FetchContent)

FetchContent_Declare(nlohmann_json URL https://gh-proxy.com/https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(nlohmann_json)

fetchcontent_declare(jwt-cpp 
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG 08bcf77a687fb06e34138e9e9fa12a4ecbe12332 # v0.7.0 release
)
set(JWT_BUILD_EXAMPLES OFF CACHE BOOL "disable building examples" FORCE)
fetchcontent_makeavailable(jwt-cpp)

file(GLOB_RECURSE SERVER_SOURCES 
    CONFIGURE_DEPENDS 
    "src/*.cpp"
)

file(GLOB_RECURSE SERVER_EXCLUDED_SOURCES 
    CONFIGURE_DEPENDS 
    "src/client/*.cpp"
)

list(REMOVE_ITEM SERVER_SOURCES ${SERVER_EXCLUDED_SOURCES})

add_executable(file_server_server
    ${SERVER_SOURCES}
)


target_link_libraries(file_server_server
    PUBLIC
    lockfreequeue
    mysqlclient
    nlohmann_json::nlohmann_json
    ssl
    crypto
    jwt-cpp::jwt-cpp
)


file(GLOB_RECURSE CLIENT_SOURCES 
    CONFIGURE_DEPENDS 
    "src/*.cpp"
)

file(GLOB_RECURSE CLIENT_EXCLUDED_SOURCES 
    CONFIGURE_DEPENDS 
    "src/server/*.cpp"
    "src/concurrency/*.cpp"
    "src/handlers/*.cpp"
    "src/net/*.cpp"
    "src/session/*.cpp"
    "src/storage/*.cpp"
)

list(REMOVE_ITEM CLIENT_SOURCES ${CLIENT_EXCLUDED_SOURCES})

add_executable(file_server_client
    ${CLIENT_SOURCES}
    src/net/epoll_poller.cpp
)
target_link_libraries(file_server_client
    lockfreequeue
    nlohmann_json::nlohmann_json
    ssl
    crypto
    mysqlclient
)
# Add tests
# add_subdirectory(tests)