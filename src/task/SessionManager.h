#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <cpr/cpr.h>

#include <string>

class SessionManager {
public:
    SessionManager();

    [[nodiscard]] cpr::Session& getSession() const noexcept;
    void resetSession();

    std::string samlResponse;
    std::string samlRequest;
    std::string uniqueSessionId;
    void generateUniqueSessionId();

private:
    std::unique_ptr<cpr::Session> m_session;
};

#endif // SESSIONMANAGER_H