#include "register_client_handler.h"
#include <iostream>

void RegisterClientHandler::handle() {
    // interactive registration: prompt username/password locally
    std::string username;
    std::cout << "用户名: " << std::flush;
    std::cin >> username;

    // disable echo for password
    std::string password;
    std::cout << "密码: " << std::flush;
    termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    std::cin >> password;
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    std::cout << std::endl;

    std::string client_hash = PasswordHash::clientHash(password);
    auto& rsa = RSAKeyManager::getInstance();
    std::string username_enc = rsa.encrypt(username);
    std::string passhash_enc = rsa.encrypt(client_hash);
    request_ = {
        {"command", "register"},
        {"params", {
            {"username", username_enc},
            {"passhash", passhash_enc}
        }}
    };
    send(MessageType::REQUEST, request_);
}
