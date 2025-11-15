#include "task/TaskLogger.h"
#include "util/Utility.h"

#include <fmt/format.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <algorithm>
#include <filesystem>
#include <ranges>
#include <utility>
#include <vector>

namespace {
void compressLog(const std::filesystem::path& txtPath) {
    if (txtPath.empty()) {
        return;
    }

    const auto console = spdlog::get("console");
    if (!std::filesystem::exists(txtPath) || !std::filesystem::is_regular_file(txtPath)) {
        console->error("Could not compress log file '{}': File does not exist or is not a regular file.",
            txtPath.filename().string());
        return;
    }

    std::filesystem::path gzPath = txtPath;
    gzPath.replace_extension(".tar.gz");
    std::filesystem::remove(gzPath);

    const std::string cmd = fmt::format(R"(tar -C "{}" -czf "{}" "{}")",
        txtPath.parent_path().string(), gzPath.string(), txtPath.filename().string());
    if (const int rc = std::system(cmd.c_str()); rc != 0) {
        console->error("Failed to tar.gz {} (exit code) {}", txtPath.filename().string(), rc);
        return;
    }

    std::filesystem::remove(txtPath);
}

std::vector<spdlog::sink_ptr> makeSinks(const bool logFile, const bool printIds, const std::filesystem::path& path) {
    std::vector<spdlog::sink_ptr> sinks;

    const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern(printIds ? "[%n] %v" : "%v");
    consoleSink->set_level(spdlog::level::info);
    sinks.push_back(consoleSink);

    if (logFile) {
        const auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
        fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        fileSink->set_level(spdlog::level::debug);
        sinks.push_back(fileSink);
    }

    return sinks;
}

std::string makeTaskId(std::string base, const std::string_view termCode) {
    switch (termCode.back()) {
        case '1':
            return base += "-FH";
        case '2':
            return base += "-DA";
        default:
            throw std::runtime_error{fmt::format("Unknown school in term code: {}", termCode)};
    }
}
} // namespace

TaskLogger::TaskLogger(std::string cwid, const std::string_view termCode, const bool logFile, const bool printIds)
    : m_taskID{makeTaskId(std::move(cwid), termCode)} {
    if (logFile) {
        const std::filesystem::path logsDirectory = getExecutableDirectory() + "/logs";
        std::filesystem::create_directory(logsDirectory);

        auto timestamp = getCurrentLocalTime();
        std::ranges::replace(timestamp, ':', '-');
        std::ranges::replace(timestamp, ' ', '_');
        m_filePath = logsDirectory / fmt::format("{}_{}.txt", m_taskID, timestamp);
    }

    auto sinks = makeSinks(logFile, printIds, m_filePath);
    m_logger = std::make_shared<spdlog::async_logger>(m_taskID, sinks.begin(), sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    m_logger->set_level(spdlog::level::debug);
    m_logger->flush_on(spdlog::level::info);
    spdlog::register_logger(m_logger);
}

TaskLogger::~TaskLogger() {
    m_logger->flush();
    spdlog::drop(m_taskID);
    compressLog(m_filePath);
}