#ifndef TASKCONFIG_H
#define TASKCONFIG_H

#include "util/Course.h"

#include <string>

struct TaskConfig {
    std::string username;
    std::string password;
    std::string term;
    std::string termCode;
    bool consoleDisplayCWID = true;
    bool enableLogging = true;
    bool watchForOpenSeats = true;
    bool enableNotifications = false;
    std::string discordWebhook;
    bool notifyFailures = false;

    std::string path;
};

#endif // TASKCONFIG_H