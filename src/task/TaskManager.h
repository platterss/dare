#ifndef TASKWATCHER_H
#define TASKWATCHER_H

#include "task/Task.h"
#include "util/Utility.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <efsw/efsw.hpp>

struct TaskHandle {
    std::unique_ptr<Task> task;
    std::future<void> future;
};

class TaskManager final : public efsw::FileWatchListener {
public:
    TaskManager();
    ~TaskManager() override;

    void start();
    void stop();
    void handleFileAction(efsw::WatchID watchID, const std::string& dir, const std::string& filename,
        efsw::Action action, std::string oldName) override;

private:
    void add(const std::filesystem::path& path);
    void remove(const std::filesystem::path& path);

    void loadInitialTasks();
    void launchTask(const std::filesystem::path& path);
    void cleanUpFinishedTasks();
    bool shouldContinue() const noexcept;
    void monitorTasks();

    std::atomic<bool> m_shutdownRequested{false};
    std::condition_variable m_shutdownCv;
    std::mutex m_mutex;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_lastEventTimes;
    std::vector<TaskHandle> m_handles;
    efsw::FileWatcher m_fileWatcher;
    efsw::WatchID m_watchId;

    static constexpr std::chrono::milliseconds DEBOUNCE_DURATION{200};
    const std::filesystem::path m_configDirectory = getExecutableDirectory() + "/configs";
};

#endif // TASKWATCHER_H