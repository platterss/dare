#include "task/CourseManager.h"
#include "data/Links.h"
#include "util/Exceptions.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <ranges>

namespace {
std::string extractCourseCode(const rapidjson::Document& json) {
    // The JSON looks something like this:
    //
    // {
    //     "subject": "COMM",
    //     "courseTitle": "INTRODUCTION TO PUBLIC SPEAKING - HONORS",
    //     "sequenceNumber": "1HW",
    //     "courseNumber": "F01AH",
    //     "responseDisplay": "INTRODUCTION TO PUBLIC SPEAKING - HONORS COMM C1000H, 1HW",
    //     "olr": false,
    //     "projectionError": false,
    //     "success": true
    // }
    //
    // `responseDisplay` shares most of its content with `courseTitle`, so we can
    // just strip `courseTitle` from the beginning of `responseDisplay` and then get
    // rid of the comma and the section number that comes after it.
    //
    // We do it this way instead of just adding `courseTitle` + `courseNumber` because the
    // courseNumber still uses the old course numbering system instead of the CCN system.

    std::string_view courseCode{json["responseDisplay"].GetString(), json["responseDisplay"].GetStringLength()};

    int startingOffset = 1; // +1 to remove the space after the title
    if (courseCode.contains("&amp;")|| courseCode.contains("&#39;")) {
        startingOffset += 4;
    }

    courseCode.remove_prefix(json["courseTitle"].GetStringLength() + startingOffset);
    courseCode.remove_suffix(json["sequenceNumber"].GetStringLength() + 2); // +2 to remove ", " before section number

    // The dot looks ugly when we put it next to a hyphen
    if (courseCode.back() == '.') {
        courseCode.remove_suffix(1);
    }

    return std::string{courseCode};
}

std::string getCourseCode(cpr::Session& session, const std::string& termCode, CRN& crn) {
    const auto response = sendRequest(session, RequestMethod::GET, Link::Classes::SECTION_DETAILS,
        cpr::Parameters{
            {"courseReferenceNumber", crn.value},
            {"term", termCode}
        }
    );

    // Only really happens if the CRN doesn't exist for the given term.
    // It's a nice way to minimize requests since we don't have to do a separate validation check.
    const rapidjson::Document json = parseJsonResponse(response.text);
    if (json.HasMember("success") && !json["success"].GetBool()) {
        throw UnrecoverableException{fmt::format("Failed to get course details for CRN {}.", crn.value)};
    }

    return extractCourseCode(json);
}
} // namespace

CourseManager::CourseManager(std::vector<Course>&& courses) : m_courses(std::move(courses)) {}

void CourseManager::populateCourseDetails(cpr::Session& session, const std::string& termCode) {
    std::vector<std::string> invalidCourses;

    auto attemptAssignment = [&](CRN& crn) {
        try {
            crn.courseCode = getCourseCode(session, termCode, crn);
        } catch (const UnrecoverableException&) { // Always returns HTTP 500 upon error
            invalidCourses.push_back(crn.value);
        }
    };

    for (Course& course : m_courses) {
        attemptAssignment(course.primary);
        std::ranges::for_each(course.backups, attemptAssignment);

        if (!course.drop.empty()) {
            attemptAssignment(course.drop);
        }
    }

    if (!invalidCourses.empty()) {
        throw UnrecoverableException{
            fmt::format("The following CRNs are invalid or not available for the term: {}", invalidCourses)
        };
    }
}

void CourseManager::displayCourses(const TaskLogger& logger) const {
    for (const Course& course : m_courses) {
        logger.info("{} (Primary)", course.primary);

        for (const CRN& backup : course.backups) {
            logger.info("{} (Backup for {})", backup, course.primary.value);
        }

        if (!course.drop.empty()) {
            logger.info("{} (Dropping for {})", course.drop, course.primary.value);
        }
    }
}

std::vector<std::pair<std::string, std::string>>& CourseManager::getNotificationQueue() {
    return m_notificationQueue;
}

void CourseManager::enqueueNotification(std::string title, std::string message) {
    // Discord notifications should not block the execution of the registration flow
    m_notificationQueue.emplace_back(std::move(title), std::move(message));
}

std::unordered_set<std::string>& CourseManager::getRegistrationQueue() noexcept {
    return m_registrationQueue;
}

void CourseManager::enqueueCRN(const std::string& crn) {
    m_registrationQueue.insert(crn);
}

void CourseManager::dequeueCRN(const std::string& crn) {
    m_registrationQueue.erase(crn);
}

void CourseManager::removeCourse(const std::string& crn) {
    const auto it = std::ranges::find_if(m_courses, [&](const Course& course) {
        return course == crn;
    });

    if (it != m_courses.end()) {
        m_courses.erase(it);
    }
}

std::vector<Course>& CourseManager::getCourses() noexcept {
    return m_courses;
}

void CourseManager::enqueueDrop(const std::string& crn) {
    m_dropQueue.insert(crn);
}

void CourseManager::dequeueDrop(const std::string& crn) {
    m_dropQueue.erase(crn);
}

std::unordered_set<std::string>& CourseManager::getDropQueue() noexcept {
    return m_dropQueue;
}

void CourseManager::setOldModel(rapidjson::Document&& model) noexcept {
    m_oldModel = std::move(model);
}

rapidjson::Document& CourseManager::getOldModel() noexcept {
    return m_oldModel;
}

void CourseManager::resetFailedCount() noexcept {
    m_failedCourses = 0;
}

void CourseManager::incrementFailedCount() noexcept {
    ++m_failedCourses;
}

bool CourseManager::hasFailures() const noexcept {
    return m_failedCourses != 0;
}

void CourseManager::clearQueues() noexcept {
    m_registrationQueue.clear();
    m_dropQueue.clear();
}

rapidjson::MemoryPoolAllocator<>& CourseManager::getAllocator() noexcept {
    return m_allocator;
}
