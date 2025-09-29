#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <cerrno>

namespace db {


class DBError : public std::runtime_error {
protected:
    std::string m_formatted_what; 

public:
    DBError(const std::string& message)
        : std::runtime_error(message) {
        
        m_formatted_what = "[Database Error] " + message;
    }
    const char* what() const noexcept override {
        return m_formatted_what.c_str();
    }
};

class FileNotExist : public DBError {
protected:
    std::string m_formatted_what; 

public:
    FileNotExist(const std::string& message)
        : DBError(message) {
        m_formatted_what = "[File Not Exist] " + message;
    }
    const char* what() const noexcept override {
        return m_formatted_what.c_str();
    }
};


} // namespace db