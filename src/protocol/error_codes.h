#ifndef ERROR_CODES_H
#define ERROR_CODES_H

// Error codes for the cloud disk application
enum class ErrorCode {
    SUCCESS = 0,
    USER_NOT_FOUND = 1,
    FILE_NOT_FOUND = 2,
    INVALID_CREDENTIALS = 3,
    PERMISSION_DENIED = 4,
    FILE_UPLOAD_FAILED = 5,
    FILE_DOWNLOAD_FAILED = 6,
    DATABASE_ERROR = 7,
    UNKNOWN_ERROR = 8
};

#endif // ERROR_CODES_H