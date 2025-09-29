#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <cerrno>

namespace storage {


class FileError : public std::runtime_error {
private:
    std::string m_formatted_what; 

public:
    FileError(const std::string& message)
        : std::runtime_error(message) {
        
        m_formatted_what = "[File Error] " + message;
    }
    const char* what() const noexcept override {
        return m_formatted_what.c_str();
    }
};


} // namespace storage