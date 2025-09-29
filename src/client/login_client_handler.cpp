#include "login_client_handler.h"
#include <iostream>
#include "client.h"

void LoginClientHandler::handle() {
    std::string username;
    std::cout << "用户名: " << std::flush;
    std::cin >> username;
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
        {"command", "login"},
        {"params", {
            {"username", username_enc},
            {"passhash", passhash_enc}
        }}
    };
    send(MessageType::REQUEST, request_);
}

void LoginClientHandler::receive() {
    Handler::receive();
    if (response_.contains("responseMessage")) {
        try {
            auto inner = nlohmann::json::parse(response_["responseMessage"].get<std::string>());
            if (inner.contains("token")) {
                std::string t = inner["token"].get<std::string>();
                std::string uname = inner.value("username", "");
                std::cout << "登录成功, 用户=" << uname << std::endl;
                if (auto c = Client::instance()) {
                    c->setToken(t);
                    if (!uname.empty()) c->setUsername(uname);
                }
            }
        } catch (...) {}
    }
}
