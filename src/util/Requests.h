#ifndef REQUESTS_H
#define REQUESTS_H

#include "task/Task.h"

#include <cpr/cpr.h>

enum class RequestMethod {
    GET,
    POST,
    HEAD
};

// Sends an HTTP request using the provided session, method, and URL.
// Assumes some content type is already set in the session (if any).
cpr::Response sendRequest(cpr::Session& session, RequestMethod method, std::string_view url);

template <typename T>
concept CprContent =
    std::same_as<std::remove_cvref_t<T>, cpr::Body> ||
    std::same_as<std::remove_cvref_t<T>, cpr::BodyView> ||
    std::same_as<std::remove_cvref_t<T>, cpr::Payload> ||
    std::same_as<std::remove_cvref_t<T>, cpr::Parameters>;

// Sends an HTTP request using the provided session, method, URL, and content (Body, BodyView, Parameters, Payload).
template <CprContent C>
cpr::Response sendRequest(cpr::Session& session, const RequestMethod method, const std::string_view url, C&& content) {
    session.SetOption(std::forward<C>(content));
    auto response = sendRequest(session, method, url);

    session.RemoveContent();
    session.SetParameters(cpr::Parameters{});

    return response;
}

// Sends a Discord notification to the Task's webhook URL.
void sendDiscordNotification(const Task& task, const std::string& title, const std::string& message);

// Checks if MyPortal is down.
bool portalIsDown();

// Gets the url encoded headers used for most requests.
cpr::Header getDefaultHeaders();

// Gets the headers used for sending requests with JSON bodies.
cpr::Header getJsonHeaders();

#endif // REQUESTS_H