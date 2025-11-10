#ifndef REGISTRATIONUTIL_H
#define REGISTRATIONUTIL_H

#include "task/Task.h"

#include <cpr/session.h>

#include <string>

// Outputs the duration of a task stage given a start time and stage name.
void logDuration(const TaskLogger& logger, std::chrono::steady_clock::time_point start, std::string_view stage);

// Waits until the portal is back online.
void waitOutError(Task& task, const std::string& message);

// Notifies the user of a failure through the logger and Discord (if enabled).
void notifyFailure(const Task& task, const std::string& title, const std::string& message);

// Visits the registration dashboard.
void visitRegistrationDashboard(cpr::Session& session);

// Visits the class registration page and returns its HTML content.
std::string visitClassRegistration(cpr::Session& session);

// Authenticates the user, checks CRNs, and waits until the user's registration time.
void prepareTask(Task& task);

// Sets up the task for the registration flow.
void prepareForRegistration(Task& task);

#endif // REGISTRATIONUTIL_H