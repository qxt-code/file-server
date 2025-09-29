#include "response_queue.h"
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <memory>

#include "common/debug.h"
#include "i_event_handler.h"
#include "types/message.h"
#include "lockfreequeue/array_mpmc_queue.hpp"

namespace net {


bool ResponseQueue::submit(int fd, Message&& response) {
    if (push(fd, std::move(response))) {
        uint64_t value = 1;
        ssize_t n = ::write(eventfd_, &value, sizeof(value));
        if (n != sizeof(value)) {
            error_cpp20("Failed to write to eventfd: " +  std::string(strerror(errno)));
            return false;
        }
        return true;
    } else {
        error_cpp20("Response queue is full, dropping response");
        return false;
    }
}

void ResponseQueue::on_readable() {
    uint64_t count;
    ssize_t n = ::read(eventfd_, &count, sizeof(count));
    if (n != sizeof(count)) {
        error_cpp20("Failed to read from eventfd:" + std::string(strerror(errno)));
    }
    for (uint64_t i = 0; i < count; ++i) {
        ResponseTask task;
        if (pop(task)) {
            Message& msg = task.message;
            log_cpp20("Sending message to fd " + std::to_string(task.fd) + ": " + 
                      std::string(reinterpret_cast<char*>(msg.body), msg.header.length));
            int fd = task.fd;
            ssize_t nsent = ::send(fd, &msg, sizeof(msg.header) + msg.header.length, MSG_WAITALL);
            if (nsent < 0) {
                error_cpp20("Failed to send message: " + std::string(strerror(errno)));
            }
            log_cpp20("Header length " + std::to_string(msg.header.length) + " Sent " + std::to_string(nsent) + " bytes to fd " + std::to_string(fd));
        }
    }
}

} // namespace net