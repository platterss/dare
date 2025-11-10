#include "TaskManager.h"
#include "registration/Register.h"
#include "registration/RegistrationUtil.h"
#include "util/Exceptions.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <fmt/format.h>

#include <exception>
#include <expected>
#include <iostream>

namespace {
bool isTxtFile(const std::filesystem::path& path) {
    return path.extension() == ".txt";
}

std::filesystem::path getPathFromName(const std::string& directory, const std::string& name) {
    return std::filesystem::path{directory} / name;
}

using ExpectedTask = std::expected<std::unique_ptr<Task>, std::string>;
ExpectedTask createTask(const std::filesystem::path& configPath) {
    if (!std::filesystem::exists(configPath)) {
        return ExpectedTask{std::unexpect, "Config file not found: " + configPath.filename().string()};
    }

    if (!isTxtFile(configPath)) {
        return ExpectedTask{std::unexpect, "Skipping non.txt file: " + configPath.filename().string()};
    }

    try {
        return ExpectedTask{std::in_place, std::make_unique<Task>(configPath.string())};
    } catch (const std::exception& e) {
        return ExpectedTask{std::unexpect, fmt::format("Error creating task from {}: {}",
            configPath.filename().string(), e.what())};
    }
}

std::future<void> launchAsyncTask(Task& task) {
    return std::async(std::launch::async, [&task] {
        try {
            prepareTask(task);
            registrationLoop(task);
        } catch (const TaskCancelled& e) {
            notifyFailure(task, "Task Cancelled", e.what());
        } catch (const std::exception& e) {
            notifyFailure(task, "Exiting Task", e.what());
        }
    });
}
} // namespace

TaskManager::TaskManager() {
    m_watchId = m_fileWatcher.addWatch(m_configDirectory.string(), this, false);
    if (m_watchId == 0) {
        throw std::runtime_error{"Failed to watch config directory: " + m_configDirectory.string()};
    }
}

TaskManager::~TaskManager() {
    stop();
}

void TaskManager::start() {
    loadInitialTasks();

    if (m_handles.empty()) {
        const auto console = spdlog::get("console");
        console->info("Please set up at least one valid configuration file in the 'configs' directory.");
        console->info("For guidance, check the wiki at https://github.com/platterss/dare/wiki/Configuration");
        // console->info("Press Enter to exit the program.");
        // std::cin.get();
        return;
    }

    m_fileWatcher.watch();
    monitorTasks();
}

void TaskManager::stop() {
    m_fileWatcher.removeWatch(m_watchId);
    m_shutdownRequested.store(true);
    m_shutdownCv.notify_all();
}

void TaskManager::handleFileAction(efsw::WatchID watchID, const std::string& dir, const std::string& filename,
        const efsw::Action action, std::string oldName) {
    const std::filesystem::path filePath = getPathFromName(dir, filename);

    {
        const std::string canonical = std::filesystem::weakly_canonical(filePath).string();
        std::lock_guard lock{m_mutex};
        const auto now = std::chrono::steady_clock::now();

        if (const auto it = m_lastEventTimes.find(canonical);
            it != m_lastEventTimes.end() && now - it->second < DEBOUNCE_DURATION) {
            return;
        }

        m_lastEventTimes[canonical] = now;
    }

    if (!isTxtFile(filePath)) {
        return;
    }

    const auto console = spdlog::get("console");
    switch (action) {
        case efsw::Actions::Add:
            console->info("Config file added: {}", filePath.filename().string());
            add(filePath);
            break;
        case efsw::Actions::Delete:
            console->info("Config file removed: {}", filePath.filename().string());
            remove(filePath);
            break;
        case efsw::Actions::Modified:
            console->info("Config file modified: {}", filePath.filename().string());
            remove(filePath);
            add(filePath);
            break;
        case efsw::Actions::Moved:
            console->info("Config file moved/renamed from {} to {}", oldName, filePath.string());
            remove(getPathFromName(dir, oldName));
            add(filePath);
            break;
    }
}

void TaskManager::add(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path) || !isTxtFile(path)) {
        return;
    }

    {
        std::lock_guard lock{m_mutex};
        for (const auto& [task, future] : m_handles) {
            if (task->config.path == path.string()) {
                spdlog::get("console")->info("Task for {} already exists. Skipping.", path.filename().string());
                return;
            }
        }
    }

    launchTask(path);
}
void TaskManager::remove(const std::filesystem::path& path) {
    const auto console = spdlog::get("console");

    std::lock_guard lock{m_mutex};
    for (auto it = m_handles.begin(); it != m_handles.end(); ++it) {
        if (it->task->config.path != path.string()) {
            continue;
        }

        console->info("Stopping task for {}", path.filename().string());
        it->task->scheduler.requestStop();

        if (it->future.valid()) {
            console->info("Waiting for task to finish: {}", path.filename().string());
            it->future.wait();
        }

        m_handles.erase(it);
        return;
    }

    console->warn("No existing task found for {}", path.filename().string());
}

void TaskManager::loadInitialTasks() {
    if (!std::filesystem::exists(m_configDirectory) || !std::filesystem::is_directory(m_configDirectory)) {
        return;
    }

    static constexpr std::chrono::seconds WAIT_TIME{5};
    while (portalIsDown()) {
        spdlog::get("console")->error("Portal is down. Trying again in {} seconds.", WAIT_TIME.count());

        std::unique_lock lock{m_mutex};
        m_shutdownCv.wait_for(lock, WAIT_TIME, [&] {
            return m_shutdownRequested.load();
        });

        if (m_shutdownRequested.load()) {
            return;
        }
    }

    for (const auto& entry : std::filesystem::directory_iterator{m_configDirectory}) {
        launchTask(entry.path());
    }
}

void TaskManager::launchTask(const std::filesystem::path& path) {
    auto task = createTask(path);

    if (!task) {
        spdlog::get("console")->error(task.error());
        return;
    }

    TaskHandle handle;
    handle.task = std::move(*task);
    handle.task->config.path = path.string();
    handle.future = launchAsyncTask(*handle.task);

    std::lock_guard lock{m_mutex};
    m_handles.push_back(std::move(handle));
}

void TaskManager::cleanUpFinishedTasks() {
    static constexpr std::chrono::seconds WAIT_TIME{0};
    std::vector<TaskHandle> ready;

    {
        std::lock_guard lock{m_mutex};
        for (auto it = m_handles.begin(); it != m_handles.end();) {
            if (it->future.valid() && it->future.wait_for(WAIT_TIME) == std::future_status::ready) {
                ready.push_back(std::move(*it));
                it = m_handles.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (auto& [task, future] : ready) {
        try {
            if (future.valid()) {
                future.get();
            }
        } catch (const std::exception& e) {
            spdlog::get("console")->error("Error in task {}: {}", task->config.path, e.what());
        }
    }
}

// Runs unlocked
bool TaskManager::shouldContinue() const noexcept {
    return !m_handles.empty() && !m_shutdownRequested.load();
}

void TaskManager::monitorTasks() {
    static constexpr std::chrono::seconds WAIT_TIME{1};

    while (true) {
        {
            std::unique_lock lock{m_mutex};
            m_shutdownCv.wait_for(lock, WAIT_TIME, [&] {
                return !shouldContinue();
            });
        }

        if (std::lock_guard lock{m_mutex}; !shouldContinue()) {
            break;
        }

        cleanUpFinishedTasks();
    }

    spdlog::get("console")->info("Shutting down.");

    std::vector<TaskHandle> remaining;
    if (std::lock_guard lock{m_mutex}; m_shutdownRequested.load() && !m_handles.empty()) {
        remaining = std::move(m_handles);
        m_handles.clear();
    }

    for (auto& [task, future] : remaining) {
        task->scheduler.requestStop();
        if (future.valid()) {
            future.wait();
        }
    }

    cleanUpFinishedTasks();
}