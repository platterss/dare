#ifndef UTILITY_H
#define UTILITY_H

#include <rapidjson/document.h>

#include <ranges>
#include <string>
#include <string_view>
#include <vector>

rapidjson::Document parseJsonResponse(std::string_view response);
rapidjson::Document parseJsonResponse(std::string_view response, rapidjson::MemoryPoolAllocator<>* allocator);
std::string getCurrentLocalTime();
std::string getCurrentUTCTime();
std::string getExecutableDirectory();

// Accepts a string in the format MM/DD/YYYY HH:MM AM (ex. 07/24/2025 10:00 AM)
std::string convert12HourTo24Hour(std::string_view time12);

// Clamps the view to the text between (begin, end)
std::string_view clampBetween(std::string_view sv, std::string_view begin, std::string_view end);

// Trims characters from both ends. Defaults to trimming whitespace.
std::string_view trimSurroundingChars(std::string_view sv, std::string_view chars = " \t\v\r\n");

int parseInt(std::string_view sv);
std::string determinePlural(std::size_t size);
std::string formatCourseCode(const std::string& subject, const std::string& courseNumber);
std::vector<std::string> split(std::string_view str, std::string_view delimiter);

#endif // UTILITY_H