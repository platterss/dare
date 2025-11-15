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
std::string_view getCourseSectionWarning(const std::string_view termCode, const std::string_view displayName) {
    using namespace std::literals;

    static constexpr std::pair<std::string_view, std::string_view> foothillSectionCodes[] = {
        {"MP"sv, "This course is only open to students in the Math Performance Success program."sv},
        {"C"sv, "This course requires you to enroll in a corequisite. See the course description for more details."sv},
        {"D"sv, "This course is only open to certain high school students. Normal college students cannot register."sv}
    };

    static constexpr std::pair<std::string_view, std::string_view> deAnzaSectionCodes[] = {
        {"FY"sv, "This course is only open to students in the First Year Experience program."sv},
        {"MP"sv, "This course is only open to students in the Math Performance Success program."sv},
        {"UM"sv, "This course is only open to students in the Umoja program."sv},
        {"A"sv, "This course is only open to students in the Study Abroad program."sv},
        {"C"sv, "This course is only open to students in the CDE Apprenticeship program."sv},
        {"D"sv, "This course is in the Learning in Communities program and requires a corequisite."sv},
        {"G"sv, "This course is only open to certain high school students. Normal college students cannot register."sv},
        {"H"sv, "This course is only open to EOPS/CARE/Next Up/Guardian Scholars."sv},
        {"J"sv, "This course is an Internship/Externship class."sv},
        {"K"sv, "This course is only open to students in the LEAD program."sv},
        {"L"sv, "This course is only open to students in the CYLC/Social Justice program."sv},
        {"M"sv, "This course is only open to students in the Mellon Scholars program."sv},
        {"N"sv, "This course is only open to students in the International Students program."sv},
        {"P"sv, "This course is only open to students in the Puente program."sv},
        {"Q"sv, "This course requires you to enroll in a corequisite. See the course description for more details."sv},
        {"R"sv, "This course is only open to students in the REACH program or is a Special Projects course."sv},
        {"S"sv, "This course is only open to students in the Pride Learning Community."sv},
        {"T"sv, "This course is only open to students in the Older Adult program."sv},
        {"V"sv, "This course is only open to students in the IMPACT AAPI program."sv},
        {"W"sv, "This course is only open to students in the FLOW program."sv}
    };

    const std::string_view sectionNumber = displayName.substr(displayName.rfind(", ") + 2);

    // Foothill
    if (termCode.back() == '1') {
        for (auto [code, definition] : foothillSectionCodes) {
            if (sectionNumber.contains(code)) {
                return definition;
            }
        }
    }

    // De Anza
    for (auto [code, definition] : deAnzaSectionCodes) {
        if (sectionNumber.contains(code)) {
            return definition;
        }
    }

    return {};
}

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

rapidjson::Document getCourseData(cpr::Session& session, const std::string& termCode, CRN& crn) {
    const auto response = sendRequest(session, RequestMethod::GET, Link::Classes::SECTION_DETAILS,
        cpr::Parameters{
            {"courseReferenceNumber", crn.value},
            {"term", termCode}
        }
    );

    // Only really happens if the CRN doesn't exist for the given term.
    // It's a nice way to minimize requests since we don't have to do a separate validation check.
    rapidjson::Document json = parseJsonResponse(response.text);
    if (json.HasMember("success") && !json["success"].GetBool()) {
        throw UnrecoverableException{fmt::format("Failed to get course details for CRN {}.", crn.value)};
    }

    return json;
}
} // namespace

CourseManager::CourseManager(std::vector<Course>&& courses) : m_courses(std::move(courses)) {
    for (const Course& course : m_courses) {
        if (course.waitlist) {
            m_waitlists.insert(course.primary.value);

            for (const CRN& backup : course.backups) {
                m_waitlists.insert(backup.value);
            }
        }
    }
}

void CourseManager::populateCourseDetails(cpr::Session& session, const std::string& termCode) {
    std::vector<std::string> invalidCourses;

    auto populateDetails = [&](CRN& crn) {
        if (crn.empty()) {
            return;
        }

        try {
            const rapidjson::Document courseData = getCourseData(session, termCode, crn);
            crn.courseCode = extractCourseCode(courseData);
            crn.sectionWarning = getCourseSectionWarning(termCode, courseData["responseDisplay"].GetString());
        } catch (const UnrecoverableException&) { // Always returns HTTP 500 upon error
            invalidCourses.push_back(crn.value);
        }
    };

    for (Course& course : m_courses) {
        populateDetails(course.primary);
        std::ranges::for_each(course.backups, populateDetails);
        populateDetails(course.drop);
    }

    if (!invalidCourses.empty()) {
        throw UnrecoverableException{
            fmt::format("The following CRNs are invalid or not available for the term: {}", invalidCourses)
        };
    }
}

void CourseManager::displayCourses(const TaskLogger& logger) const {
    auto printWarning = [&](const CRN& crn) {
        if (crn.sectionWarning.empty()) {
            return;
        }

        logger.info("[WARNING] {} (CRN {}) has enrollment restrictions: {}",
            crn.courseCode, crn.value, crn.sectionWarning);
    };

    for (const Course& course : m_courses) {
        logger.info("{} (Primary)", course.primary);
        printWarning(course.primary);

        for (const CRN& backup : course.backups) {
            logger.info("{} (Backup for {})", backup, course.primary.value);
            printWarning(backup);
        }

        if (!course.drop.empty()) {
            logger.info("{} (Dropping for {})", course.drop, course.primary.value);
        }
    }
}

bool CourseManager::canWaitlistCourse(const std::string& crn) const {
    return m_waitlists.contains(crn);
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
        m_waitlists.erase(it->primary.value);
        for (const CRN& backup : it->backups) {
            m_waitlists.erase(backup.value);
        }

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
