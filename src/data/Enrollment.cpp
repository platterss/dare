#include "data/Enrollment.h"
#include "data/Links.h"
#include "data/Regexes.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <ctre.hpp>
#include <fmt/format.h>

#include <string_view>
#include <unordered_map>
#include <utility>

namespace {
SeatType getSeatType(const std::string_view seatTypeName) {
    if (seatTypeName == "Enrollment Actual") {
        return SeatType::EnrollmentActual;
    }

    if (seatTypeName == "Enrollment Maximum") {
        return SeatType::EnrollmentMaximum;
    }

    if (seatTypeName == "Enrollment Seats Available") {
        return SeatType::EnrollmentSeatsAvailable;
    }

    if (seatTypeName == "Waitlist Actual") {
        return SeatType::WaitlistActual;
    }

    if (seatTypeName == "Waitlist Capacity") {
        return SeatType::WaitlistCapacity;
    }

    if (seatTypeName == "Waitlist Seats Available") {
        return SeatType::WaitlistSeatsAvailable;
    }

    throw std::invalid_argument{fmt::format("Unrecognized seat type name: {}", seatTypeName)};
}

std::string getEnrollmentHtml(const std::string& termCode, const std::string& crn) {
    static constexpr int MAX_ATTEMPTS = 3;
    for (int attempts = 1; attempts <= MAX_ATTEMPTS; ++attempts) {
        try {
            cpr::Session session;
            session.SetHeader(getDefaultHeaders());

            return sendRequest(session, RequestMethod::POST, Link::Classes::ENROLLMENT_INFO,
                cpr::Parameters{
                    {"term", termCode},
                    {"courseReferenceNumber", crn}
                }
            ).text;
        } catch (const std::exception& e) {
            if (attempts == MAX_ATTEMPTS) {
                throw std::runtime_error{fmt::format("[{}] Error getting course information ({})", crn, e.what())};
            }
        }
    }

    std::unreachable();
}

std::vector<int> getClassEnrollmentInfo(const std::string& termCode, const std::string& crn) {
    std::vector<int> enrollmentData(+SeatType::Size);

    for (const auto& match : ctre::search_all<Regex::Enrollment::ENROLLMENT_DATA>(getEnrollmentHtml(termCode, crn))) {
        enrollmentData[+getSeatType(match.get<1>())] = parseInt(match.get<2>());
    }

    return enrollmentData;
}
} // namespace

std::string EnrollmentInfo::getDescription() const {
    switch (status) {
        using enum CourseStatus;
        using enum SeatType;

        case Open:
            return fmt::format("Open - Seats Available: {}", seats[+EnrollmentSeatsAvailable]);
        case WaitlistOpen:
            return fmt::format("Waitlist - Seats Available: {}", seats[+WaitlistSeatsAvailable]);
        case WaitlistSoon:
            return fmt::format("Waitlist - Seats Opening Soon: {}",
                seats[+EnrollmentSeatsAvailable] + seats[+WaitlistSeatsAvailable]);
        case Closed:
            return std::string{"Closed - No Seats Available"};
    }

    std::unreachable();
}

EnrollmentInfo checkEnrollmentAvailability(const std::string& termCode, const std::string& crn) {
    using enum CourseStatus;
    using enum SeatType;

    std::vector<int> data = getClassEnrollmentInfo(termCode, crn);

    if (data[+EnrollmentSeatsAvailable] > 0 && data[+WaitlistActual] == 0) {
        return {Open, std::move(data)};
    }

    if (data[+WaitlistSeatsAvailable] > 0) {
        return {WaitlistOpen, std::move(data)};
    }

    // Student(s) dropped from main roster, but system hasn't moved waitlisted student(s) in yet
    // Waitlisted seats can be negative sometimes for some reason so we have to balance it out
    if (data[+EnrollmentSeatsAvailable] + data[+WaitlistSeatsAvailable] > 0) {
        return {WaitlistSoon, std::move(data)};
    }

    return {Closed, std::move(data)};
}