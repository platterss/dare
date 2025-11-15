#ifndef TASKCONFIG_H
#define TASKCONFIG_H

#include "util/Course.h"

#include <string>

struct TaskConfig {
    std::string cwid;
    std::string password;
    std::string term;
    std::string termCode;
    bool displayCwid = true;
    bool enableLogs = true;
    bool watchForOpenSeats = true;
    bool enableNotifications = false;
    std::string discordWebhook;

    std::string path;
};

#endif // TASKCONFIG_H