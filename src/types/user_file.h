#pragma once

#include <string>
#include <chrono>
#include "enums.h"

struct UserFile {
    size_t id{0};                  // Unique identifier for the file
    size_t userId{0};             // Owner user ID
    size_t parentId{0};            // Identifier of the parent directory
    size_t fileId{0};               // File identifier
    std::string fileName{""};       // Name of the file
    std::string filePath{""};       // Path to the file
    FileType fileType{0};       // Type of the file (FILE, DIRECTORY, SYMLINK)
    std::chrono::system_clock::time_point createdTime; // Creation time
    std::chrono::system_clock::time_point modifiedTime; // Last modified time
    bool isDeleted{false};          // Indicates if the file is deleted

    UserFile(const std::string& name, const std::string& path)
        : fileName(name), filePath(path) {
        createdTime = std::chrono::system_clock::now();
        modifiedTime = createdTime;
    }

    void updateModifiedTime() {
        modifiedTime = std::chrono::system_clock::now();
    }
};
