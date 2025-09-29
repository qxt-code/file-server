#pragma once

#include <string>
#include <chrono>

struct FileMetadata {
    size_t id;                  // Unique identifier for the file
    std::string hashCode;    // Hash code of the file
    size_t fileSize;            // Size of the file in bytes
    size_t receivedBytes;      // Number of bytes received (for transfers)
    size_t sentBytes;          // Number of bytes sent (for transfers)
    std::chrono::system_clock::time_point createdTime; // Creation time
    std::chrono::system_clock::time_point modifiedTime; // Last modified time
    size_t refCount;           // Reference count for the file

    FileMetadata() 
        : id(0), hashCode(""), fileSize(0), receivedBytes(0), sentBytes(0), refCount(0) {}
    FileMetadata(std::string hash, size_t size)
        : hashCode(hash), fileSize(size), refCount(1) {
        createdTime = std::chrono::system_clock::now();
        modifiedTime = createdTime;
    }

    void updateModifiedTime() {
        modifiedTime = std::chrono::system_clock::now();
    }
};
