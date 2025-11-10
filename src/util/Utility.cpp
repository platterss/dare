#include "util/Utility.h"

#include <fmt/format.h>

#include <charconv>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

rapidjson::Document parseJsonResponse(const std::string_view response) {
    rapidjson::Document json;
    json.Parse(response.data(), response.size());

    if (json.HasParseError()) {
        throw std::runtime_error{"Failed to parse JSON response: " + std::to_string(json.GetParseError())};
    }

    return json;
}

rapidjson::Document parseJsonResponse(const std::string_view response, rapidjson::MemoryPoolAllocator<>* allocator) {
    rapidjson::Document json{allocator};
    json.Parse(response.data(), response.size());

    if (json.HasParseError()) {
        throw std::runtime_error{"Failed to parse JSON response: " + std::to_string(json.GetParseError())};
    }

    return json;
}

std::string getCurrentLocalTime() {
    const auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    localtime_s(&tm, &now_time);
#else
    localtime_r(&now_time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string getCurrentUTCTime() {
    const auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};

#ifdef _WIN32
    gmtime_s(&tm, &now_time);
#else
    gmtime_r(&now_time, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string getExecutableDirectory() {
    char path[1024];

#if defined(_WIN32)
    GetModuleFileNameA(NULL, path, sizeof(path));
#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0) {
        std::cerr << "Error getting executable path on macOS." << '\n';
        return "";
    }
#elif defined(__linux__)
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
    if (count == -1) {
        std::cerr << "Error getting executable path on Linux." << '\n';
        return "";
    }
    path[count] = '\0';
#endif

    return std::filesystem::canonical(path).parent_path().string();
}

std::string convert12HourTo24Hour(const std::string_view time12) {
    const auto firstSpace = time12.find(' ');
    if (firstSpace == std::string_view::npos) {
        throw std::runtime_error{fmt::format("Missing space after date in: {}", time12)};
    }

    const auto secondSpace = time12.find(' ', firstSpace + 1);
    if (secondSpace == std::string_view::npos) {
        throw std::runtime_error{fmt::format("Missing space after time in: {}", time12)};
    }

    const std::string_view datePart = time12.substr(0, firstSpace);
    const std::string_view timePart = time12.substr(firstSpace + 1, secondSpace - firstSpace - 1);
    const std::string_view meridiem = time12.substr(secondSpace + 1);

    const auto colonPos = timePart.find(':');
    if (colonPos == std::string_view::npos) {
        throw std::runtime_error{fmt::format("Missing colon after time in: {}", time12)};
    }

    int hour = parseInt(timePart.substr(0, colonPos));
    int mins = parseInt(timePart.substr(colonPos + 1));

    if (meridiem == "PM" && hour != 12) {
        hour += 12;
    } else if (meridiem == "AM" && hour == 12) {
        hour = 0;
    }

    return fmt::format("{} {:02}:{:02}", datePart, hour, mins);
}

std::string_view clampBetween(const std::string_view sv, const std::string_view begin, const std::string_view end) {
    const auto startPos = sv.find(begin);
    if (startPos == std::string_view::npos) {
        throw std::runtime_error{"Failed to find start position in string_view"};
    }
    const auto beginOffset = startPos + begin.size();

    const auto endPos = sv.find(end, beginOffset);
    if (endPos == std::string_view::npos) {
        throw std::runtime_error{"Failed to find end position in string_view"};
    }
    return sv.substr(beginOffset, endPos - beginOffset);
}

std::string_view trimSurroundingChars(const std::string_view sv, const std::string_view chars) {
    const auto start = sv.find_first_not_of(chars);
    if (start == std::string_view::npos) {
        return {};
    }

    const auto end = sv.find_last_not_of(chars);
    return sv.substr(start, end - start + 1);
}

int parseInt(const std::string_view sv) {
    int value{};

    if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
            ec != std::errc{} || ptr != sv.data() + sv.size()) {
        throw std::runtime_error{fmt::format("Invalid integer: {}", sv)};
            }

    return value;
}

std::string determinePlural(const std::size_t size) {
    return size == 1 ? "" : "s";
}

std::string formatCourseCode(const std::string& subject, const std::string& courseNumber) {
    std::string courseCode = subject + ' ' + courseNumber;

    // The dot looks ugly when we put a hyphen next to it
    if (courseCode.back() == '.') {
        courseCode.pop_back();
    }

    return courseCode;
}

std::vector<std::string> split(const std::string_view str, const std::string_view delimiter) {
    std::vector<std::string> result;

    for (auto&& part : std::ranges::views::split(str, delimiter)) {
        result.emplace_back(std::ranges::begin(part), std::ranges::end(part));
    }

    return result;
}