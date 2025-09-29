#include <csignal>
#include <functional>
#include <unistd.h>

#include "client.h"

std::function<void(int)> g_signal_handler;

void handle_signal(int signal) {
    if (g_signal_handler) {
        g_signal_handler(signal);
    }
}

int main() {
    Client client;
    g_signal_handler = [&client](int signal) {
        client.stop();
    };
    // ::signal(SIGINT, handle_signal);

    client.connect("127.0.0.1", 8000);
    client.run();
    return 0;
}