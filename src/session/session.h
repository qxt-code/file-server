#pragma once

#include <string>
#include <unordered_map>
#include <memory>

#include "types/user.h"
#include "types/context.h"
#include "storage/file_manager.h"

namespace session {

class Session {
public:
    Session(const std::string& sessionId, const User& user);
    const std::string& getSessionId() const;
    const User& getUser() const;
    void setCurrentFile(const std::string& filePath);
    const std::string& getCurrentFile() const;

private:
    std::string sessionId; 
    User user;
    std::string currentFile;
};

using SessionPtr = std::shared_ptr<Session>;

class SessionManager {
public:
    SessionManager();
    SessionPtr createSession(const std::string& sessionId, const User& user);
    void destroySession(const std::string& sessionId);
    SessionPtr getSession(const std::string& sessionId) const;

private:
    std::unordered_map<std::string, SessionPtr> sessions;
};

} // namespace storage