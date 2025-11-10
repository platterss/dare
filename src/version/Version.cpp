#include "version/Version.h"
#include "data/Links.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <cpr/cpr.h>
#include <rapidjson/document.h>
#include <spdlog/spdlog.h>

#include <string>
#include <tuple>

namespace {
std::tuple<int, int, int> parseVersion(const std::string& version) {
    const std::size_t start = version.front() == 'v' ? 1 : 0;
    const std::size_t dot1 = version.find('.', start);
    const std::size_t dot2 = version.find('.', dot1 + 1);

    if (dot1 == std::string::npos || dot2 == std::string::npos) {
        throw std::runtime_error{"Invalid version string: " + version};
    }

    int major = std::stoi(version.substr(start, dot1 - start));
    int minor = std::stoi(version.substr(dot1 + 1, dot2 - dot1 - 1));
    int patch = std::stoi(version.substr(dot2 + 1));

    return {major, minor, patch};
}

bool atLeastCurrent(const std::tuple<int, int, int>& version) {
    const auto [remoteMajor, remoteMinor, remotePatch] = version;

    if (PROJECT_VERSION_MAJOR > remoteMajor) {
        return true;
    }

    if (PROJECT_VERSION_MAJOR < remoteMajor) {
        return false;
    }

    if (PROJECT_VERSION_MINOR > remoteMinor) {
        return true;
    }

    if (PROJECT_VERSION_MINOR < remoteMinor) {
        return false;
    }

    return PROJECT_VERSION_PATCH >= remotePatch;
}
} // namespace

void checkVersion() {
    const auto console = spdlog::get("console");

    std::string latestVersion;
    try {
        cpr::Session session;
        const auto responseText = sendRequest(session, RequestMethod::GET, Link::GitHub::REPO_LATEST_RELEASE).text;

        const rapidjson::Document json = parseJsonResponse(responseText);
        latestVersion = {json["tag_name"].GetString(), json["tag_name"].GetStringLength()};
    } catch (const std::exception& e) {
        console->error("Could not get latest version information from GitHub: {}", e.what());
        return;
    }

    if (const auto version = parseVersion(latestVersion); !atLeastCurrent(version)) {
        console->info("A new version of DARE is available ({}). You are on v{}.", latestVersion, PROJECT_VERSION);
        console->info("You can find the latest version at https://github.com/platterss/dare.");
        console->info("Updating is strongly recommended as there may have been MyPortal changes that break older versions of DARE.");
        console->info("Or maybe there are just some cool new features and bug fixes. Check the changelog for more information.");
    }
}