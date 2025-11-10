#include "task/TaskScheduler.h"
#include "data/Links.h"
#include "data/Regexes.h"
#include "util/Exceptions.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <fmt/format.h>

#include <ctre.hpp>
#include <date/tz.h>

namespace {
std::chrono::system_clock::time_point parseTime(const std::string_view timeStr) {
    std::istringstream iss{convert12HourTo24Hour(timeStr)};

    date::local_time<std::chrono::seconds> localTimePoint;
    iss >> date::parse("%m/%d/%Y %H:%M", localTimePoint);

    if (iss.fail()) {
        throw std::runtime_error{fmt::format("Failed to parse time: {}", timeStr)};
    }

    // Portal displays in Los Angeles time
    const auto laZone = date::locate_zone("America/Los_Angeles");
    const date::zoned_time zonedTimePoint{laZone, localTimePoint};

    return zonedTimePoint.get_sys_time();
}

std::string fetchRegistrationTimeHTML(cpr::Session& session, const std::string& term, const std::string& sessionId) {
    sendRequest(session, RequestMethod::POST, Link::Reg::TERM_CONFIRM_PRE_REG,
        cpr::Payload{
            {"term", term},
            {"studyPath", ""},
            {"studyPathText", ""},
            {"startDatepicker", ""},
            {"endDatepicker", ""},
            {"uniqueSessionId", sessionId}
        }
    );

    return sendRequest(session, RequestMethod::GET, Link::Reg::PREPARE_REG).text;
}

std::string extractRegistrationTime(const std::string_view html) {
    if (const auto& match = ctre::search<Regex::Scheduler::REGISTRATION_TIME>(html)) {
        return match.to_string();
    }

    throw std::runtime_error{"Error getting registration time."};
}
} // namespace

void TaskScheduler::saveRegistrationTime(cpr::Session& session, const std::string& term, const std::string& sessionId) {
    const std::string html = fetchRegistrationTimeHTML(session, term, sessionId);

    if (!html.contains("Please register within these times")) {
        throw UnrecoverableException{
            "You are not eligible to register for this term. Make sure you have submitted an application."
        };
    }

    if (!html.contains("You have no holds which prevent registration.")) {
        throw UnrecoverableException{
            "You have holds on your account which prevent registration. Please resolve them before proceeding."
        };
    }

    m_registrationTimeStr = extractRegistrationTime(html);
    m_registrationTimePoint = parseTime(m_registrationTimeStr);
}

const std::string& TaskScheduler::getRegistrationTime() const noexcept {
    return m_registrationTimeStr;
}

std::chrono::system_clock::time_point TaskScheduler::getRegistrationTimePoint() const noexcept {
    return m_registrationTimePoint;
}

void TaskScheduler::sleepUntilReauthentication(const TaskLogger& logger) {
    using namespace std::chrono;
    static constexpr auto SECONDS_BEFORE = 5s;
    const auto waitDuration = duration_cast<seconds>(m_registrationTimePoint - SECONDS_BEFORE - system_clock::now());

    if (waitDuration < 0s) {
        return;
    }

    pause(logger, waitDuration, "before reauthenticating");
}

void TaskScheduler::sleepUntilOpen(const TaskLogger& logger) {
    using namespace std::chrono;
    const auto waitDuration = duration_cast<seconds>(m_registrationTimePoint - system_clock::now());

    if (waitDuration < 0s) {
        return;
    }

    pause(logger, waitDuration, "for registration to open");
}

void TaskScheduler::requestStop() noexcept {
    m_stopRequested.store(true);
    m_stopCv.notify_all();
}

void TaskScheduler::throwIfStopped() const {
    if (m_stopRequested.load()) {
        throw TaskCancelled{};
    }
}

void TaskScheduler::pause(const TaskLogger& logger, const std::chrono::duration<double> dur, const std::string& msg) {
    if (!msg.empty()) {
        const std::chrono::hh_mm_ss hms{dur};
        logger.info("Pausing for {:02}h {:02}m {:02}s {}.",
            hms.hours().count(), hms.minutes().count(), hms.seconds().count(), msg);
    }

    std::unique_lock lock{m_stopMutex};
    if (m_stopCv.wait_for(lock, dur, [this] { return m_stopRequested.load(); })) {
        logger.info("Stop requested. Waking up early.");
    }
}