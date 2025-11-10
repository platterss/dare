#ifndef CONFIGLOADER_H
#define CONFIGLOADER_H

#include "task/TaskConfig.h"
#include "util/Course.h"

#include <string_view>
#include <utility>
#include <vector>

struct ConfigLoader {
    static std::pair<TaskConfig, std::vector<Course>> load(std::string_view configPath);
};

#endif // CONFIGLOADER_H