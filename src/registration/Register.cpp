#include "registration/Register.h"
#include "auth/Authentication.h"
#include "data/Enrollment.h"
#include "data/Links.h"
#include "registration/RegistrationUtil.h"
#include "task/Task.h"
#include "util/Exceptions.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <fmt/format.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include <algorithm>
#include <array>
#include <expected>
#include <optional>
#include <random>
#include <ranges>
#include <unordered_set>
#include <vector>

namespace {
using CrnRef = std::reference_wrapper<CRN>;
using AddDropPair = std::pair<CrnRef, std::optional<std::string>>;

void notifyResults(Task& task) {
    for (const auto& [title, message] : task.courseManager.getNotificationQueue()) {
        sendDiscordNotification(task, title, message);
    }

    task.courseManager.getNotificationQueue().clear();
}

std::optional<std::string_view> ineligibleToRegisterReason(const std::string_view regMessage) {
    static constexpr std::array<std::string_view, 12> INELIGIBLE_ERROR_MESSAGES = {
        "Corequisite",
        "Prereq not met",
        "Class passed. No repeats",
        "Time conflict. Registration prohibited",
        "Exceeded unit maximum",
        "The add period is over",
        "Duplicate Course",
        "Duplicate Equivalent",
        "Authorization required",
        "Cohort Restriction",
        "Program Restriction",
        "Special Projects"
    };

    for (const auto& errorMessage : INELIGIBLE_ERROR_MESSAGES) {
        if (regMessage.contains(errorMessage)) {
            return std::optional<std::string_view>{std::in_place, errorMessage};
        }
    }

    return std::optional<std::string_view>{std::nullopt};
}

std::string getDescription(const std::string_view statusDescription) {
    if (statusDescription == "Deleted") {
        return std::string{"Dropped"};
    }

    return std::string{statusDescription};
}

void processUpdate(Task& task, const rapidjson::Value& update) {
    std::string crn{update["courseReferenceNumber"].GetString(), update["courseReferenceNumber"].GetStringLength()};

    if (!task.courseManager.getRegistrationQueue().contains(crn) && !task.courseManager.getDropQueue().contains(crn)) {
        return;
    }

    std::string courseCode = formatCourseCode(
        std::string{update["subject"].GetString(), update["subject"].GetStringLength()},
        std::string{update["courseDisplay"].GetString(), update["courseDisplay"].GetStringLength()}
    );
    std::string message = fmt::format("[{}] {} - ", crn, courseCode);

    if (const std::string status = getDescription(update["statusDescription"].GetString());
            status == "Registered" || status == "Waitlisted" || status == "Dropped") {
        message += "Successfully " + status;
        task.courseManager.removeCourse(crn);
    } else if (status == "Errors Preventing Registration") {
        const auto& error = update["messages"][0]["message"]; // First one is usually the most important
        std::string_view errorMessage{error.GetString(), error.GetStringLength()};

        if (const auto reason = ineligibleToRegisterReason(errorMessage)) {
            errorMessage = *reason;
            task.courseManager.removeCourse(crn);
        }

        task.courseManager.incrementFailedCount();
        message += fmt::format("{} - {}", status, errorMessage);
    }

    task.logger.info(message);
    task.courseManager.enqueueNotification(std::move(courseCode), std::move(message));
}

void reviewBatchResponse(Task& task, const std::string_view response) {
    task.scheduler.throwIfStopped();

    const rapidjson::Document batchResponse = parseJsonResponse(response);
    if (!batchResponse.HasMember("success") || !batchResponse["success"].GetBool()) {
        throw std::runtime_error{"Batch response was unsuccessful."};
    }

    for (const auto& update : batchResponse["data"]["update"].GetArray()) {
        processUpdate(task, update);
    }

    task.courseManager.clearQueues();
    task.courseManager.getOldModel().Clear();
    task.courseManager.getAllocator().Clear();
}

std::string sendBatch(const Task& task, const std::string_view batch) {
    task.scheduler.throwIfStopped();

    const auto startTime = std::chrono::steady_clock::now();

    cpr::Session& session = task.sessionManager.getSession();
    session.SetHeader(getJsonHeaders());

    std::string respText = sendRequest(session, RequestMethod::POST, Link::Reg::BATCH, cpr::BodyView{batch}).text;

    session.SetHeader(getDefaultHeaders());

    task.logger.info("Sent registration request.");
    logDuration(task.logger, startTime, "Sending batch");

    return respText;
}

void addDropsToBatch(Task& task, rapidjson::Document& batch) {
    task.scheduler.throwIfStopped();

    std::vector<std::string> dropsToRemove;
    for (const std::string& dropCrn : task.courseManager.getDropQueue()) {
        bool currentlyEnrolledInDrop = false;

        for (auto& model : task.courseManager.getOldModel().GetArray()) {
            if (model["courseReferenceNumber"].GetString() != dropCrn) {
                continue;
            }

            currentlyEnrolledInDrop = true;
            task.logger.info("Enqueuing CRN {} to drops.", dropCrn);

            // When we're currently registered for a course, the only actions we have are:
            // - "DW" (***Web Dropped***), and
            // - "null" (None).
            model["selectedAction"].SetString("DW", batch.GetAllocator());
            batch["update"].PushBack(model, batch.GetAllocator());
            break;
        }

        if (!currentlyEnrolledInDrop) {
            dropsToRemove.push_back(dropCrn);
        }
    }

    for (const std::string& crn : dropsToRemove) {
        task.courseManager.dequeueDrop(crn);
        task.logger.info("Not currently enrolled in CRN {}. Removing from drop queue.", crn);
    }
}

void handleFailedAdd(Task& task, const rapidjson::Value& val) {
    const std::string crn{val["courseReferenceNumber"].GetString(), val["courseReferenceNumber"].GetStringLength()};

    task.courseManager.removeCourse(crn);
    task.courseManager.dequeueCRN(crn);

    std::string message = fmt::format("[{}] Error adding course: {}", crn, val["message"].GetString());
    task.logger.error(message);
    task.courseManager.enqueueNotification("Error Adding Course", std::move(message));
}

void addCoursesToBatch(Task& task, rapidjson::Document& cart, rapidjson::Document& batch) {
    for (auto& course : cart["aaData"].GetArray()) {
        if (!course["success"].GetBool()) {
            handleFailedAdd(task, course);
            continue;
        }

        // For a course we're about to register for, there are three possible registration actions:
        // - "RW" (**Web Registered**), which is the default option,
        // - "WL" (Waitlist), and
        // - "internal-remove" (Remove).
        // "RW" and "internal-remove" always appear, but "WL" only appears if the course is waitlisted.
        auto& model = course["model"];
        if (model["properties"]["registrationActions"].Size() == 3 &&
            task.courseManager.canWaitlistCourse(model["courseReferenceNumber"].GetString())) {
            model["selectedAction"].SetString("WL", cart.GetAllocator());
        }

        batch["update"].PushBack(model, batch.GetAllocator());
    }
}

rapidjson::Document createBatch(const std::string& sessionId, rapidjson::MemoryPoolAllocator<>* allocator) {
    rapidjson::Document batch{allocator};
    batch.SetObject();

    batch.AddMember("create", rapidjson::Value{rapidjson::kArrayType}, *allocator);
    batch.AddMember("destroy", rapidjson::Value{rapidjson::kArrayType}, *allocator);
    batch.AddMember("uniqueSessionId", rapidjson::Value{}.SetString(sessionId.c_str(), sessionId.size()), *allocator);
    batch.AddMember("update", rapidjson::Value{rapidjson::kArrayType}, *allocator);

    return batch;
}

std::string createCrnString(const std::unordered_set<std::string>& crnList) {
    std::string crnString;

    // crnList is guaranteed to have at least one CRN
    static constexpr std::size_t CRN_LENGTH = 6; // Includes the comma
    crnString.reserve(crnList.size() * CRN_LENGTH - 1); // -1 to remove the comma after the last CRN

    for (const std::string& crn : crnList) {
        if (!crnString.empty()) {
            crnString += ',';
        }

        crnString += crn;
    }

    return crnString;
}

rapidjson::Document addCrnRegistrationItems(Task& task) {
    const auto now = std::chrono::steady_clock::now();

    const auto response = sendRequest(task.sessionManager.getSession(), RequestMethod::POST,
        Link::Reg::ADD_CRN_REG_ITEMS,
        cpr::Payload{
            {"crnList", createCrnString(task.courseManager.getRegistrationQueue())},
            {"term", task.config.termCode}
        }
    );

    logDuration(task.logger, now, "Adding CRNs to cart");
    task.logger.info("Added CRNs to cart.");

    return parseJsonResponse(response.text, &task.courseManager.getAllocator());
}

std::expected<std::string, std::string> prepareBatch(Task& task) {
    task.scheduler.throwIfStopped();

    rapidjson::Document crnCart = addCrnRegistrationItems(task);
    rapidjson::Document batch = createBatch(task.sessionManager.uniqueSessionId, &task.courseManager.getAllocator());

    addCoursesToBatch(task, crnCart, batch);
    if (batch["update"].Empty()) {
        return std::expected<std::string, std::string>{std::unexpect, "Added no courses to batch."};
    }

    addDropsToBatch(task, batch);
    if (const std::size_t dropQueueSize = task.courseManager.getDropQueue().size(); dropQueueSize > 0) {
        task.logger.info("Added {} course{} to drop queue.", dropQueueSize, determinePlural(dropQueueSize));
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer{buffer};
    batch.Accept(writer);

    return std::expected<std::string, std::string>{std::in_place, buffer.GetString(), buffer.GetLength()};
}

void finalizeRegistration(Task& task) {
    const std::size_t queueSize = task.courseManager.getRegistrationQueue().size();
    if (queueSize == 0) {
        return;
    }

    task.logger.info("Added {} course{} to registration queue.", queueSize, determinePlural(queueSize));

    const auto startTime = std::chrono::steady_clock::now();
    prepareForRegistration(task);

    const auto batch = prepareBatch(task);
    if (!batch) {
        logDuration(task.logger, startTime, "Registration (no courses)");
        task.logger.error(batch.error());
        return;
    }

    const std::string batchResponse = sendBatch(task, *batch);
    logDuration(task.logger, startTime, "Registration");

    reviewBatchResponse(task, batchResponse);
}

CrnRef selectBestCandidate(const std::vector<CrnRef>& candidates, const bool prioritizeOpenSeats) {
    auto bestIt = candidates.begin();

    if (prioritizeOpenSeats) {
        for (auto candIt = candidates.begin(); candIt != candidates.end(); ++candIt) {
            if (candIt->get().enrollmentInfo.status == CourseStatus::Open) {
                return *candIt;
            }

            const int candSeats = candIt->get().enrollmentInfo.seats[+SeatType::WaitlistActual];
            const int bestSeats = bestIt->get().enrollmentInfo.seats[+SeatType::WaitlistActual];

            if (candSeats < bestSeats) {
                bestIt = candIt;
            }
        }
    }

    return *bestIt;
}

bool crnIsAddable(const CourseStatus status, const bool waitlistConsideredAddable) {
    return status == CourseStatus::Open || (status == CourseStatus::WaitlistOpen && waitlistConsideredAddable);
}

std::vector<CrnRef> getCandidates(const Task& task, Course& course) {
    std::vector<CrnRef> toCheck;
    toCheck.reserve(1 + course.backups.size());
    toCheck.emplace_back(course.primary);

    for (auto& backup : course.backups) {
        toCheck.emplace_back(backup);
    }

    std::vector<std::future<void>> futures;
    futures.reserve(toCheck.size());
    for (auto& check : toCheck) {
        futures.emplace_back(std::async(std::launch::async, [&] {
            CRN& crn = check.get();
            crn.enrollmentInfo = checkEnrollmentAvailability(task.config.termCode, crn.value);
            task.logger.info("{} - {}", crn, crn.enrollmentInfo.getDescription());
        }));
    }

    for (auto& future : futures) {
        future.get();
    }

    std::vector<CrnRef> candidates;
    for (auto& result : toCheck) {
        if (crnIsAddable(result.get().enrollmentInfo.status, course.waitlist)) {
            candidates.emplace_back(result.get());
        }
    }

    return candidates;
}

std::optional<AddDropPair> processCourse(Task& task, Course& course) {
    const std::vector<CrnRef> candidates = getCandidates(task, course);

    if (candidates.empty()) {
        task.courseManager.incrementFailedCount();
        return {};
    }

    CrnRef best = selectBestCandidate(candidates, course.prioritizeOpenSeats);

    if (!course.drop.empty()) {
        return std::optional<AddDropPair>{std::in_place, best, course.drop.value};
    }

    return std::optional<AddDropPair>{std::in_place, best, std::nullopt};
}

void processCourses(Task& task) {
    std::vector<std::future<std::optional<AddDropPair>>> futures;
    futures.reserve(task.courseManager.getCourses().size());

    for (Course& course : task.courseManager.getCourses()) {
        futures.emplace_back(std::async(std::launch::async, [&] {
            return processCourse(task, course);
        }));
    }

    for (auto& future : futures) {
        if (auto result = future.get()) {
            auto& [best, drop] = *result;

            task.courseManager.enqueueCRN(best.get().value);
            if (drop) {
                task.courseManager.enqueueDrop(*drop);
            }

            task.logger.info("Enqueuing {}.", best.get());
        }
    }

    finalizeRegistration(task);
    notifyResults(task);
}

void sleepForRandomTime(Task& task, std::uniform_real_distribution<>& dist, std::mt19937& gen) {
    const double sleepDuration = dist(gen);
    task.logger.info("Checking again in {:.2f} seconds.", sleepDuration);
    task.scheduler.pauseFor(task.logger, std::chrono::duration<double>{sleepDuration});
    task.scheduler.throwIfStopped();
}

// Returns true if processed courses without errors, false if encountered a recoverable error.
// If error is unrecoverable or the user cancelled the task, it'll just throw.
bool attemptRegistration(Task& task) {
    try {
        processCourses(task);
    } catch (const TaskCancelled&) {
        throw;
    } catch (const UnrecoverableException& e) {
        notifyFailure(task, "Error", e.what());
        throw;
    } catch (const std::exception& e) {
        task.logger.error("Error - {}.", e.what());
        task.courseManager.resetFailedCount();
        waitOutError(task, e.what());
        return false;
    }

    return true;
}
} // namespace

void registrationLoop(Task& task) {
    static constexpr double MIN_WAIT_SECONDS = 3.0;
    static constexpr double MAX_WAIT_SECONDS = 6.0;
    static constexpr int REAUTHENTICATE_AFTER = 500;

    std::random_device rd;
    std::mt19937 gen{rd()};
    std::uniform_real_distribution timeDist{MIN_WAIT_SECONDS, MAX_WAIT_SECONDS};

    int attempts = 0;

    while (!task.courseManager.getCourses().empty()) {
        if (!attemptRegistration(task)) {
            continue;
        }

        // Either registered for everything or courses were unaddable
        if (task.courseManager.getCourses().empty() || !task.courseManager.hasFailures()) {
            break;
        }

        // Some classes were full but user does not want to watch for open seats
        if (!task.config.watchForOpenSeats) {
            break;
        }

        if (++attempts >= REAUTHENTICATE_AFTER) {
            authenticate(task);
            attempts = 0;
        }

        task.courseManager.resetFailedCount();
        sleepForRandomTime(task, timeDist, gen);
    }
}