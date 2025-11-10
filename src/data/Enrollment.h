#ifndef ENROLLMENT_H
#define ENROLLMENT_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class CourseStatus {
    Closed,
    Open,
    WaitlistOpen,
    WaitlistSoon
};

enum class SeatType {
    EnrollmentActual,           // Number of students in main roster
    EnrollmentMaximum,          // Maximum number of students allowed in main roster
    EnrollmentSeatsAvailable,   // Number of seats available in main roster
    WaitlistActual,             // Number of students on waitlist
    WaitlistCapacity,           // Maximum number of students allowed on waitlist
    WaitlistSeatsAvailable,     // Number of seats available in waitlist
    Size
};

// Returns the underlying integer for a SeatType enum
constexpr int operator+(const SeatType type) noexcept {
    return std::to_underlying(type);
}

struct EnrollmentInfo {
    CourseStatus status = CourseStatus::Closed;
    std::vector<int> seats;

    EnrollmentInfo() = default;
    EnrollmentInfo(const CourseStatus status, std::vector<int> seats) noexcept
        : status(status), seats{std::move(seats)} {}

    // Gets a printable description of the enrollment status and seats available (if any).
    [[nodiscard]] std::string getDescription() const;
};

// Checks the enrollment availability for a given term and CRN.
EnrollmentInfo checkEnrollmentAvailability(const std::string& termCode, const std::string& crn);

#endif // ENROLLMENT_H