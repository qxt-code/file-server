#pragma once

#include <string>
#include <unordered_map>

namespace protocol {

// Enum for defining command types
enum class CommandType {
    LOGIN,
    UPLOAD,
    DOWNLOAD,
    LIST,
    DELETE,
    INVALID
};

// Structure to represent a command
struct Command {
    CommandType type;
    std::string user_id;
    std::string file_name;
    std::string additional_data; // For any extra data needed for the command
};

// Function to convert string to CommandType
inline CommandType stringToCommandType(const std::string& command_str) {
    static const std::unordered_map<std::string, CommandType> command_map = {
        {"login", CommandType::LOGIN},
        {"upload", CommandType::UPLOAD},
        {"download", CommandType::DOWNLOAD},
        {"list", CommandType::LIST},
        {"delete", CommandType::DELETE}
    };

    auto it = command_map.find(command_str);
    return (it != command_map.end()) ? it->second : CommandType::INVALID;
}

} // namespace protocol