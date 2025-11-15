#include "registration/RegistrationUtil.h"
#include "auth/Authentication.h"
#include "data/Links.h"
#include "util/Requests.h"
#include "util/Utility.h"

namespace {
void registrationTermSelect(cpr::Session& session) {
    sendRequest(session, RequestMethod::HEAD, Link::Reg::TERM_SELECT_CLASS_REG);
}

std::string registrationConfirmTerm(const SessionManager& sessionManager, const std::string& termCode) {
    return sendRequest(sessionManager.getSession(), RequestMethod::POST, Link::Reg::TERM_CONFIRM_CLASS_REG,
        cpr::Payload{
            {"term", termCode},
            {"studyPath", ""},
            {"studyPathText", ""},
            {"startDatepicker", ""},
            {"endDatepicker", ""},
            {"uniqueSessionId", sessionManager.uniqueSessionId}
        }
    ).text;
}

std::string extractSummaryModels(const std::string_view responseText) {
    // This is probably not the best way to do this, but it works...
    // This little snippet is embedded within the HTML for the class registration page.
    // We're only interested in the summaryModels array, which contains all the
    // user's currently-registered courses. We can just hone in on the opening bracket
    // and the closing bracket right before the summaryDisplayConfig section.
    //
    // window.bootstraps = {
    //     ...
    //     summaryModels:
    //     [
    //     {
    //         ...
    //     }
    //     ],
    //     summaryDisplayConfig:
    //     [
    //         ...
    //     ],
    //     ...
    // };

    static constexpr std::string_view SUMMARY_MODELS_START = "summaryModels:";
    static constexpr std::string_view SUMMARY_MODELS_END = "summaryDisplayConfig";

    const std::string_view models = trimSurroundingChars(
        clampBetween(responseText, SUMMARY_MODELS_START, SUMMARY_MODELS_END)
    );

    return std::string{models.substr(0, models.size() - 1)}; // Remove the trailing comma
}

bool registrationIsOpen(const SessionManager& sessionManager, const std::string& termCode) {
    visitRegistrationDashboard(sessionManager.getSession());
    registrationTermSelect(sessionManager.getSession());

    // An example of a request from ineligible user.
    // `studentEligFailures` shows up whenever registration isn't open for the user yet,
    // whether it be that registration isn't open yet or if they are ineligible to register (hold, not enrolled, etc.).
    // We cover the ineligible case during the first authentication though.
    //
    // Whenever registration opens, the only thing here will be "fwdURL".
    //
    // {
    //     "studentEligValid": false,
    //     "studentEligFailures":
    //     [
    //       "You have no Registration Time Ticket for the current time."
    //     ],
    //     "fwdURL": "/StudentRegistrationSsb/ssb/classRegistration/classRegistration"
    // }
    const rapidjson::Document json = parseJsonResponse(registrationConfirmTerm(sessionManager, termCode));

    return !json.HasMember("studentEligFailures");
}

void waitUntilPortalOnline(Task& task) {
    using namespace std::chrono_literals;
    while (portalIsDown()) {
        task.scheduler.pauseFor(task.logger, 5s, "for portal to come back online");
    }
}
} // namespace


void logDuration(const TaskLogger& logger, const std::chrono::steady_clock::time_point start,
        const std::string_view stage) {
    const auto end = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    logger.debug("{} took {} ms", stage, duration.count());
}

void waitOutError(Task& task, const std::string& message) {
    notifyFailure(task, "Error", message);
    task.courseManager.clearQueues();
    waitUntilPortalOnline(task);

    // We ignore HTTP 502 and 504 errors since they're just temporary, likely just the server rebooting.
    // Typically only happens at 2:05/3:05 AM (depending on daylight savings).
    if (!(message.contains("HTTP 502") || message.contains("HTTP 504"))) {
        authenticate(task);
    }
}

void notifyFailure(const Task& task, const std::string& title, const std::string& message) {
    task.logger.error(title + " - " + message);
    sendDiscordNotification(task, title, message);
}

void visitRegistrationDashboard(cpr::Session& session) {
    sendRequest(session, RequestMethod::HEAD, Link::Reg::REG_DASHBOARD);
}

std::string visitClassRegistration(cpr::Session& session) {
    return sendRequest(session, RequestMethod::GET, Link::Reg::CLASS_REG).text;
}

void prepareTask(Task& task) {
    task.scheduler.throwIfStopped();

    authenticate(task);

    task.courseManager.populateCourseDetails(task.sessionManager.getSession(), task.config.termCode);
    task.courseManager.displayCourses(task.logger);

    task.logger.info("Registration time: " + task.scheduler.getRegistrationTime());

    task.scheduler.sleepUntilReauthentication(task.logger);
    authenticate(task);

    task.scheduler.sleepUntilOpen(task.logger);

    // Sometimes, registration doesn't actually open right at the time it says it does. Liars.
    while (!registrationIsOpen(task.sessionManager, task.config.termCode)) {
        using namespace std::chrono_literals;
        task.scheduler.throwIfStopped();
        task.logger.info("Registration not yet open. Waiting...");
        task.scheduler.pauseFor(task.logger, 1s);
    }

    task.logger.info("Registration is open.");
}

void prepareForRegistration(Task& task) {
    const auto startTime = std::chrono::steady_clock::now();

    authenticate(task);
    visitRegistrationDashboard(task.sessionManager.getSession());
    registrationTermSelect(task.sessionManager.getSession());
    registrationConfirmTerm(task.sessionManager, task.config.termCode);

    // Get the old set of models from the class registration page. Kind of a lot of work.
    task.courseManager.getAllocator().Clear();

    task.courseManager.setOldModel(
        parseJsonResponse(
            extractSummaryModels(
                visitClassRegistration(task.sessionManager.getSession())
            ),
            &task.courseManager.getAllocator()
        )
    );

    logDuration(task.logger, startTime, "Preparing for registration");
}