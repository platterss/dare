#ifndef TASK_H
#define TASK_H

#include "task/ConfigLoader.h"
#include "task/CourseManager.h"
#include "task/SessionManager.h"
#include "task/TaskConfig.h"
#include "task/TaskLogger.h"
#include "task/TaskScheduler.h"

#include <string_view>
#include <utility>

struct Task {
    explicit Task(const std::string_view configPath) : Task{ConfigLoader::load(configPath)} {}
    explicit Task(std::pair<TaskConfig, std::vector<Course>>&& loaded)
        : config{std::move(loaded.first)},
          courseManager{std::move(loaded.second)},
          logger{config.username, config.termCode, config.enableLogging, config.consoleDisplayCWID} {}

    TaskConfig config;
    CourseManager courseManager;
    SessionManager sessionManager;
    TaskLogger logger;
    TaskScheduler scheduler;
};

#endif // TASK_H