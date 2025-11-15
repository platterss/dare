#include "util/Requests.h"
#include "data/Links.h"
#include "util/Exceptions.h"
#include "util/Utility.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

namespace {
constexpr void checkResponseCode(const long code) {
    if (cpr::status::is_success(code) || cpr::status::is_redirect(code)) {
        return;
    }

    std::string errorMessage = "Error: HTTP " + std::to_string(code);

    if (code == 0) {
        throw UnrecoverableException{errorMessage + " - Unable to make requests. Check your internet connection."};
    }

    if (cpr::status::is_server_error(code) && portalIsDown()) {
        errorMessage += " - Portal is down.";
    }

    throw std::runtime_error{errorMessage};
}

std::string createDiscordBody(const std::string& cwid, const std::string& title, const std::string& message) {
    rapidjson::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();

    rapidjson::Value embed{rapidjson::kObjectType};
    embed.AddMember("title", rapidjson::Value(title.c_str(), allocator), allocator);
    embed.AddMember("description", rapidjson::Value(message.c_str(), allocator), allocator);
    embed.AddMember("timestamp", rapidjson::Value(getCurrentUTCTime().c_str(), allocator), allocator);

    rapidjson::Value footer{rapidjson::kObjectType};
    footer.AddMember("text", rapidjson::Value(cwid.c_str(), allocator), allocator);
    embed.AddMember("footer", footer, allocator);

    rapidjson::Value embeds{rapidjson::kArrayType};
    embeds.PushBack(embed, allocator);

    document.AddMember("username", rapidjson::StringRef("DARE"), allocator);
    document.AddMember("avatar_url", rapidjson::StringRef(Link::Discord::PROFILE_PICTURE.data()), allocator);
    document.AddMember("embeds", embeds, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer{buffer};
    document.Accept(writer);

    return std::string{buffer.GetString(), buffer.GetLength()};
}
} // namespace

cpr::Response sendRequest(cpr::Session& session, const RequestMethod method, const std::string_view url) {
    session.SetUrl(url);

    auto response = [&] {
        switch (method) {
            case RequestMethod::GET:
                return session.Get();
            case RequestMethod::POST:
                return session.Post();
            case RequestMethod::HEAD:
                return session.Head();
        }

        std::unreachable();
    }();

    checkResponseCode(response.status_code);

    return response;
}

void sendDiscordNotification(const Task& task, const std::string& title, const std::string& message) {
    if (!task.config.enableNotifications) {
        return;
    }

    cpr::Session session;
    session.SetHeader(getJsonHeaders());

    try {
        sendRequest(session, RequestMethod::POST, task.config.discordWebhook,
            cpr::Body{createDiscordBody(task.config.cwid, title, message)}
        );
    } catch (const std::exception& e) {
        task.logger.error("Discord Webhook {}", e.what());
    }
}

bool portalIsDown() {
    static constexpr std::chrono::seconds TIMEOUT{10};
    const auto response = cpr::Get(cpr::Url{Link::Reg::TERM_SELECT_CLASS_REG}, cpr::Timeout{TIMEOUT}, getDefaultHeaders());

    return response.status_code == 0 ||
        (cpr::status::is_server_error(response.status_code)  && response.text.contains("internal error"));
}

cpr::Header getDefaultHeaders() {
    static const cpr::Header HEADERS = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36"},
        {"Content-Type", "application/x-www-form-urlencoded"},
        {"Accept", "*/*"},
        {"Accept-Encoding", "gzip, deflate, br"},
        {"Accept-Language", "en-US,en;q=0.9"}
    };

    return HEADERS;
}

cpr::Header getJsonHeaders() {
    static const cpr::Header HEADERS = {
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/138.0.0.0 Safari/537.36"},
        {"Content-Type", "application/json"},
        {"Accept", "*/*"},
        {"Accept-Encoding", "gzip, deflate, br"},
        {"Accept-Language", "en-US,en;q=0.9"}
    };

    return HEADERS;
}