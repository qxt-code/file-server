#include "session.h"
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

class Session {
public:
    Session(int userId) : userId(userId) {}

    int getUserId() const {
        return userId;
    }

    void setCurrentFile(const std::string& file) {
        std::lock_guard<std::mutex> lock(mutex);
        currentFile = file;
    }

    std::string getCurrentFile() const {
        std::lock_guard<std::mutex> lock(mutex);
        return currentFile;
    }

private:
    int userId;
    std::string currentFile;
    mutable std::mutex mutex;
};

class SessionManager {
public:
    std::shared_ptr<Session> createSession(int userId) {
        std::lock_guard<std::mutex> lock(mutex);
        auto session = std::make_shared<Session>(userId);
        sessions[userId] = session;
        return session;
    }

    std::shared_ptr<Session> getSession(int userId) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = sessions.find(userId);
        return (it != sessions.end()) ? it->second : nullptr;
    }

    void removeSession(int userId) {
        std::lock_guard<std::mutex> lock(mutex);
        sessions.erase(userId);
    }

private:
    std::unordered_map<int, std::shared_ptr<Session>> sessions;
    mutable std::mutex mutex;
} sessionManager;