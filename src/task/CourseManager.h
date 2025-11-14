#ifndef COURSEMANAGER_H
#define COURSEMANAGER_H

#include "task/TaskLogger.h"
#include "util/Course.h"

#include <cpr/cpr.h>
#include <rapidjson/document.h>

#include <atomic>
#include <string>
#include <unordered_set>
#include <vector>

class CourseManager {
public:
    CourseManager() = default;
    explicit CourseManager(std::vector<Course>&& courses);

    void populateCourseDetails(cpr::Session& session, const std::string& termCode);
    void displayCourses(const TaskLogger& logger) const;
    bool canWaitlistCourse(const std::string& crn) const;

    std::vector<std::pair<std::string, std::string>>& getNotificationQueue();
    void enqueueNotification(std::string title, std::string message);

    std::unordered_set<std::string>& getRegistrationQueue() noexcept;
    void enqueueCRN(const std::string& crn);
    void dequeueCRN(const std::string& crn);
    void removeCourse(const std::string& crn);
    std::vector<Course>& getCourses() noexcept;

    void enqueueDrop(const std::string& crn);
    void dequeueDrop(const std::string& crn);
    std::unordered_set<std::string>& getDropQueue() noexcept;
    void setOldModel(rapidjson::Document&& model) noexcept;
    rapidjson::Document& getOldModel() noexcept;

    void resetFailedCount() noexcept;
    void incrementFailedCount() noexcept;
    bool hasFailures() const noexcept;
    void clearQueues() noexcept;

    rapidjson::MemoryPoolAllocator<>& getAllocator() noexcept;

private:
    std::vector<Course> m_courses;
    std::unordered_set<std::string> m_waitlists;

    std::vector<std::pair<std::string, std::string>> m_notificationQueue;

    std::unordered_set<std::string> m_registrationQueue;
    std::unordered_set<std::string> m_dropQueue;

    std::atomic_int m_failedCourses{0};

    rapidjson::MemoryPoolAllocator<> m_allocator;
    rapidjson::Document m_oldModel{&m_allocator};
};

#endif // COURSEMANAGER_H