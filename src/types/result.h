#ifndef RESULT_H
#define RESULT_H

#include <string>

enum class ResultCode {
    SUCCESS,
    FAILURE,
    NOT_FOUND,
    UNAUTHORIZED,
    INVALID_INPUT,
    SERVER_ERROR
};

struct Result {
    ResultCode code;
    std::string message;

    Result(ResultCode code, const std::string& message)
        : code(code), message(message) {}
};

#endif // RESULT_H