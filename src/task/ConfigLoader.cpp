#include "task/ConfigLoader.h"
#include "data/Terms.h"

#include <cpr/cpr.h>
#include <fmt/format.h>
#include <toml++/toml.hpp>

static bool discordWebhookValid(const std::string_view webhook) {
    static constexpr std::string_view EMPTY_WEBHOOK = "https://discord.com/api/webhooks/";

    return !webhook.empty() || webhook != EMPTY_WEBHOOK ||
        (webhook.starts_with(EMPTY_WEBHOOK) && cpr::status::is_success(cpr::Head(cpr::Url{webhook}).status_code));
}

static void validateConfig(TaskConfig& config) {
    if (config.username.empty() || config.password.empty() || config.term.empty()) {
        throw std::runtime_error{"Missing required fields in config file."};
    }

    static constexpr std::size_t USERNAME_LENGTH = 8;
    if (config.username.size() != USERNAME_LENGTH) {
        throw std::runtime_error{fmt::format("CWID has wrong length (expected {}, got {}).",
            USERNAME_LENGTH, config.username.size())};
    }

    if (!discordWebhookValid(config.discordWebhook)) {
        config.enableNotifications = false;
        config.notifyFailures = false;
    }
}

static void ensureUniqueCrn(std::unordered_set<std::string>& seenCrns, const std::string& crn) {
    if (seenCrns.contains(crn)) {
        throw std::runtime_error{"Duplicate CRN " + crn + " found in config file."};
    }

    seenCrns.insert(crn);
}

static std::vector<Course> readCourses(const toml::parse_result& parsed) {
    const auto coursesArray = parsed["Course"].as_array();

    if (!coursesArray) {
        throw std::runtime_error{"No courses listed in config file."};
    }

    std::vector<Course> courses;
    courses.reserve(coursesArray->size());

    std::unordered_set<std::string> seenCrns;
    for (auto&& courseTable : *coursesArray) {
        const auto table = courseTable.as_table();

        if (!table) {
            continue;
        }

        Course course{};

        if (const auto primary = table->get_as<std::string>("primary")) {
            ensureUniqueCrn(seenCrns, **primary);
            course.primary.value = **primary;
        } else {
            throw std::runtime_error{"Course missing required 'primary' field."};
        }

        if (const auto backupsArray = table->get_as<toml::array>("backups")) {
            course.backups.reserve(backupsArray->size());

            for (auto&& backupNode : *backupsArray) {
                if (auto backup = backupNode.value<std::string>()) {
                    ensureUniqueCrn(seenCrns, *backup);
                    course.backups.emplace_back(*backup);
                }
            }
        }

        if (const auto drop = table->get_as<std::string>("drop_on_open")) {
            ensureUniqueCrn(seenCrns, **drop);
            course.drop.value = **drop;
        }

        if (const auto prioritizeOpenSeats = table->get_as<bool>("prioritize_open_seats")) {
            course.prioritizeOpenSeats = **prioritizeOpenSeats;
        }

        courses.push_back(std::move(course));
    }

    return courses;
}

static TaskConfig readSettings(const toml::parse_result& parsed) {
    TaskConfig taskConfig;

    const auto loginInfo = parsed["Login"];
    taskConfig.username = loginInfo["username"].value_or("");
    taskConfig.password = loginInfo["password"].value_or("");

    const auto term = parsed["Term"];
    taskConfig.term = term["term"].value_or("");
    taskConfig.termCode = getTermCode(taskConfig.term);

    const auto settings = parsed["Settings"];
    taskConfig.automaticallyWaitlist = settings["automatically_waitlist"].value_or(taskConfig.automaticallyWaitlist);
    taskConfig.consoleDisplayCWID = settings["console_display_cwid"].value_or(taskConfig.consoleDisplayCWID);
    taskConfig.enableLogging = settings["enable_logging"].value_or(taskConfig.enableLogging);
    taskConfig.watchForOpenSeats = settings["watch_for_open_seats"].value_or(taskConfig.watchForOpenSeats);

    const auto notifSettings = parsed["Notifications"];
    taskConfig.enableNotifications = notifSettings["enable_notifications"].value_or(taskConfig.enableNotifications);
    taskConfig.discordWebhook = notifSettings["discord_webhook"].value_or("");
    taskConfig.notifyFailures = notifSettings["send_failure_notifications"].value_or(taskConfig.notifyFailures);

    validateConfig(taskConfig);

    return taskConfig;
}

std::pair<TaskConfig, std::vector<Course>> ConfigLoader::load(const std::string_view configPath) {
    const auto parseResult = toml::parse_file(configPath);
    return {readSettings(parseResult), readCourses(parseResult)};
}