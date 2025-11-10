#include "task/TaskManager.h"
#include "util/Utility.h"
#include "version/Version.h"

#include <date/tz.h>
#include <spdlog/async.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <csignal>
#include <memory>

namespace {
std::unique_ptr<TaskManager> g_taskManager;

void stopTaskManager() {
    if (g_taskManager) {
        g_taskManager->stop();
    } else {
        std::exit(0);
    }
}

#if defined(_WIN32)
#include <windows.h>

BOOL WINAPI handleConsoleEvent(DWORD event) {
    switch (event) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            stopTaskManager();
            return TRUE;
        default:
            return FALSE;
    }
}
#endif

#if defined(__linux__) || defined(__APPLE__)
extern "C" void stopTasks(int) {
    stopTaskManager();
}
#endif

void setupDate() {
    date::set_install(getExecutableDirectory() + "/tzdata");
}

void setupSignalHandlers() {
#if defined(_WIN32)
    SetConsoleCtrlHandler(handleConsoleEvent, TRUE);
#endif

#if defined(__linux__) || defined(__APPLE__)
    std::signal(SIGHUP, stopTasks);
    std::signal(SIGINT, stopTasks);
    std::signal(SIGTERM, stopTasks);
#endif
}

void setupLogging() {
    static constexpr std::size_t QUEUE_SIZE = 512; // Could honestly be way lower since we don't print that much
    static constexpr std::size_t THREAD_COUNT = 1;
    spdlog::init_thread_pool(QUEUE_SIZE, THREAD_COUNT);

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[DARE] %v");
    consoleSink->set_level(spdlog::level::info);

    const auto console_logger = std::make_shared<spdlog::async_logger>(
        "console",
        spdlog::sinks_init_list{consoleSink},
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    console_logger->set_level(spdlog::level::info);
    spdlog::register_logger(console_logger);
}
} // namespace

int main() {
    setupDate();
    setupLogging();
    setupSignalHandlers();

    checkVersion();

    g_taskManager = std::make_unique<TaskManager>();
    g_taskManager->start();
}