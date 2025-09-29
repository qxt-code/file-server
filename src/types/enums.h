#ifndef ENUMS_H
#define ENUMS_H

enum class UserRole {
    ADMIN,
    USER,
    GUEST
};

enum class FileType {
    FILE,
    DIRECTORY,
    SYMLINK
};

enum class FileStatus {
    UPLOADED,
    DOWNLOADED,
    DELETED,
    ERROR
};

enum class MessageType {
    REQUEST = 1,
    RESPONSE = 2,
    ERROR = 3,
    PUT_DATA = 4,
    NOTIFICATION = 5,
    GET_DATA = 6
};

enum class RequestType {
    PWD,
    LOGIN,
    UPLOAD,
    DOWNLOAD,
    LIST,
    DELETE
};

#endif // ENUMS_H