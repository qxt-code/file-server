#pragma once

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <iostream>

#include <source_location> 

#define DEBUG 0

#if defined __CPPPLUSPLUS
    extern "C" {
#endif

#if DEBUG==1
    #define DEBUG_PRINT(fmt, ...) \
        printf("[DEBUG] file: %s, function: %s, line: %d | " fmt "\n", \
               __FILE__, __func__, __LINE__  __VA_OPT__(, __VA_ARGS__))
#else
    #define DEBUG_PRINT(fmt, ...)
#endif

#define RUNTIME_ERROR(fmt, ...) \
    fprintf(stderr, "[ERROR] file: %s, function: %s, line: %d | " fmt "\n", \
            __FILE__, __func__, __LINE__ __VA_OPT__(, __VA_ARGS__))

#define ERR_CHECK(cond, msg) \
    if (cond) {            \
        RUNTIME_ERROR(msg);  \
        return -1;          \
    }
#if defined __CPPPLUSPLUS
}
#endif

inline void log_cpp20(const std::string& message, 
               const std::source_location& location = std::source_location::current()) {
#if DEBUG==1
    std::cout << "[" << location.file_name() 
              << ":" << location.line() 
              << ":" << location.column() << "] "
              << location.function_name() << "() - " 
              << message << std::endl;
#endif
}

inline void error_cpp20(const std::string& message, 
               const std::source_location& location = std::source_location::current()) {
#if DEBUG==1
    std::cout << "[" << location.file_name() 
              << ":" << location.line() 
              << ":" << location.column() << "] "
              << location.function_name() << "(ERROR) - " 
              << message << std::endl;
#endif
}