#ifndef COURSE_H
#define COURSE_H

#include "data/Enrollment.h"

#include <fmt/format.h>

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

struct CRN {
    std::string value;
    std::string courseCode;
    std::string_view sectionWarning;
    EnrollmentInfo enrollmentInfo;

    CRN() = default;
    explicit CRN(std::string crn) noexcept : value{std::move(crn)} {}

    [[nodiscard]] bool empty() const noexcept {
        return value.empty();
    }

    bool operator==(const CRN& other) const noexcept {
        return value == other.value;
    }

    bool operator==(const std::string& crn) const noexcept {
        return value == crn;
    }
};

template <>
struct fmt::formatter<CRN> : formatter<std::string> {
    auto format(const CRN& crn, format_context& ctx) const {
        return formatter<std::string>::format(fmt::format("[{}] {}", crn.value, crn.courseCode), ctx);
    }
};

struct Course {
    CRN primary;
    std::vector<CRN> backups;
    CRN drop;
    bool prioritizeOpenSeats = false;
    bool waitlist = true;

    bool operator==(const std::string& crn) const {
        return primary == crn || drop == crn ||
            std::ranges::any_of(backups, [&crn](const CRN& backup) {
                return backup == crn;
            });
    }
};

#endif // COURSE_H