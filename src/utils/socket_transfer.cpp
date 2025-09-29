#include "socket_transfer.h"

#include <string.h>
#include <sys/socket.h>
#include <errno.h>

#include "common/debug.h"


int SocketTransfer::sendAll(int socket, const char* buffer, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent = send(socket, buffer + totalSent, length - totalSent, 0);
        if (sent == 0) {
            log_cpp20("Connection closed by peer on fd " + std::to_string(socket));
            break;
        } else if (sent < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, try again
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, try again later
                log_cpp20("No data available to send on fd " + std::to_string(socket));
                return -1;
            } else {
                error_cpp20("Failed to send data. Error: " + std::string(strerror(errno)));
                return -2; // Error occurred
            }
        }
        totalSent += sent;
    }
    return totalSent;
}

int SocketTransfer::recvAll(int socket, char* buffer, size_t length) {
    size_t totalReceived = 0;
    while (totalReceived < length) {
        ssize_t received = recv(socket, buffer + totalReceived, length - totalReceived, 0);
        if (received == 0) {
            log_cpp20("Connection closed by peer on fd " + std::to_string(socket));
            break;
        } else if (received < 0) {
            if (errno == EINTR) {
                continue; // Interrupted, try again
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, try again later
                log_cpp20("No data available to send on fd " + std::to_string(socket));
                return -1;
            } else {
                error_cpp20("Failed to send data. Error: " + std::string(strerror(errno)));
                return -2; // Error occurred
            }
        }
        totalReceived += received;
    }
    return totalReceived;
}

int SocketTransfer::recvNonBlocking(int socket, char* buffer, size_t length) {

    ssize_t received = recv(socket, buffer, length, MSG_DONTWAIT);
    if (received < 0) {
        if (errno == EINTR) {
            log_cpp20("recv interrupted by signal, retrying on fd " + std::to_string(socket));
            return -1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_cpp20("No data available to read on fd " + std::to_string(socket));
            return -2;
        }
        return -3;
    } else if (received == 0) {
        log_cpp20("Connection closed by peer on fd " + std::to_string(socket));
        return 0;
    }

    return received;
}

int SocketTransfer::sendNonBlocking(int socket, char* buffer, size_t length) {

    ssize_t sent = recv(socket, buffer, length, MSG_DONTWAIT);
    if (sent < 0) {
        if (errno == EINTR) {
            log_cpp20("send interrupted by signal, retrying on fd " + std::to_string(socket));
            return -1;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log_cpp20("No data available to send on fd " + std::to_string(socket));
            return -2;
        }
        return -3;
    } else if (sent == 0) {
        return 0;
    }

    return sent;
}