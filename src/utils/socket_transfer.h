#pragma once

#include <sys/types.h>

class SocketTransfer {

public:
    static int recvAll(int socket, char* buffer, size_t length);
    
    static int sendAll(int socket, const char* buffer, size_t length);

    static int recvNonBlocking(int socket, char* buffer, size_t length);

    static int sendNonBlocking(int socket, char* buffer, size_t length);
};