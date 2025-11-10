#ifndef TASKSCHEDULER_H
#define TASKSCHEDULER_H

#include "TaskLogger.h"

#include <cpr/session.h>

#include <chrono>
#include <string>

class TaskScheduler {
public:
    void saveRegistrationTime(cpr::Session& session, const std::string& term, const std::string& sessionId);
    const std::string& getRegistrationTime() const noexcept;
    std::chrono::system_clock::time_point getRegistrationTimePoint() const noexcept;
    void sleepUntilReauthentication(const TaskLogger& logger);
    void sleepUntilOpen(const TaskLogger& logger);
    void requestStop() noexcept;
    void throwIfStopped() const;
    void pause(const TaskLogger& logger, std::chrono::duration<double> dur, const std::string& msg = "");

private:
    std::string m_registrationTimeStr;
    std::chrono::system_clock::time_point m_registrationTimePoint;

    std::atomic<bool> m_stopRequested = false;
    std::mutex m_stopMutex;
    std::condition_variable m_stopCv;
};

#endif // TASKSCHEDULER_H