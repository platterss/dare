#ifndef TASKLOGGER_H
#define TASKLOGGER_H

#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

class TaskLogger {
public:
    TaskLogger() = default;
    TaskLogger(std::string cwid, std::string_view termCode, bool logFile, bool printIds);
    ~TaskLogger();

    void debug(const std::string& str) const {
        m_logger->debug(str);
    }

    void info(const std::string& str) const {
        m_logger->info(str);
    }

    void warn(const std::string& str) const {
        m_logger->warn(str);
    }

    void error(const std::string& str) const {
        m_logger->error(str);
    }

    void critical(const std::string& str) const {
        m_logger->critical(str);
    }

    template<typename... Args>
    void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) const {
        m_logger->debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(spdlog::format_string_t<Args...> fmt, Args&&... args) const {
        m_logger->info(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        m_logger->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(spdlog::format_string_t<Args...> fmt, Args&&... args) const {
        m_logger->error(fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
        m_logger->critical(fmt, std::forward<Args>(args)...);
    }

private:
    std::string m_taskID;
    std::filesystem::path m_filePath;
    std::shared_ptr<spdlog::logger> m_logger;
};


#endif // TASKLOGGER_H