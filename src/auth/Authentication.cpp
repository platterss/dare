#include "auth/Authentication.h"
#include "data/Links.h"
#include "data/Regexes.h"
#include "registration/RegistrationUtil.h"
#include "task/SessionManager.h"
#include "util/Exceptions.h"
#include "util/Requests.h"

#include <ctre.hpp>
#include <fmt/format.h>

namespace {
// Extracts the value of the hidden input field from the HTML response.
std::string getHiddenInput(const std::string_view html) {
    // 1 is for the hidden input name (unused since the OCI migration), 2 is for the value
    static constexpr int HIDDEN_INPUT_VALUE = 2;

    // There's only ever one hidden input anymore so we don't have to use ctre::search_all.
    if (const auto& match = ctre::search<Regex::Auth::HIDDEN_INPUTS>(html)) {
        return match.get<HIDDEN_INPUT_VALUE>().to_string();
    }

    throw std::runtime_error{"Could not find hidden inputs during authentication."};
}

// Fetches and saves the user's registration time if it hasn't already been saved.
void fetchRegistrationTime(Task& task) {
    if (task.scheduler.getRegistrationTimePoint() == std::chrono::system_clock::time_point{}) {
        task.scheduler.saveRegistrationTime(task.sessionManager.getSession(), task.config.termCode,
            task.sessionManager.uniqueSessionId);
    }
}

void selfServiceSSO(SessionManager& sessionManager) {
    sendRequest(sessionManager.getSession(), RequestMethod::POST, Link::Auth::SELF_SERVICE_SSO,
        cpr::Payload{
            {"SAMLResponse", std::move(sessionManager.samlResponse)}
        }
    );
}

void login(SessionManager& sessionManager, const std::string& username, const std::string& password) {
    const auto response = sendRequest(sessionManager.getSession(), RequestMethod::POST, Link::Auth::LOGIN_PAGE,
        cpr::Payload{
            {"j_username", username},
            {"j_password", password},
            {"_eventId_proceed", ""}
        }
    );

    // If credentials are invalid, it will redirect (HTTP 302 Found) to `e1s2`,
    // `e1s3`, etc. depending on the number of unsuccessful login attempts.
    // Otherwise, it will return HTTP 200 OK.
    if (response.status_code == cpr::status::HTTP_FOUND &&
        response.header.contains("Location") &&
        response.header.at("Location").starts_with("/idp/profile/SAML2/POST/SSO?execution=e1s")) {
        throw UnrecoverableException{fmt::format(
            "Invalid credentials for CWID '{}'. Please check your username and password.", username)};
    }

    sessionManager.samlResponse = getHiddenInput(response.text);
}

void getLoginPage(cpr::Session& session) {
    sendRequest(session, RequestMethod::GET, Link::Auth::LOGIN_PAGE);
}

bool idpSSO(SessionManager& sessionManager) {
    const auto response = sendRequest(sessionManager.getSession(), RequestMethod::POST, Link::Auth::IDP_SSO,
        cpr::Payload{
            {"SAMLRequest", std::move(sessionManager.samlRequest)}
        }
    );

    // If this stage is successful, it'll redirect to the login page.
    // Very rarely, it'll redirect you to '/ssomanager/ui/error.jsp'
    // telling you there was an "Error validating SAML message."
    // The easy fix is just to clear cookies and log in again.

    // Response code is always HTTP 302 (Found) so we don't need to check it.
    return Link::Auth::LOGIN_PAGE.contains(response.header.at("Location"));
}

void ssbLoginRedirect(SessionManager& sessionManager) {
    const auto response = sendRequest(sessionManager.getSession(), RequestMethod::GET, Link::Auth::SAML_LOGIN);
    sessionManager.samlRequest = getHiddenInput(response.text);
}

bool alreadyAuthenticated(cpr::Session& session) {
    const auto response = sendRequest(session, RequestMethod::GET, Link::Auth::AUTH_AJAX);

    // If authenticated, it'll redirect (HTTP 302 Found) to the registration dashboard
    // If not, it'll return "userNotLoggedIn" in the response body (with HTTP 200 OK)
    return response.status_code == cpr::status::HTTP_FOUND;
}
} // namespace

void authenticate(Task& task) {
    task.scheduler.throwIfStopped();

    if (alreadyAuthenticated(task.sessionManager.getSession())) {
        task.logger.debug("Already authenticated. Skipping login.");
        return;
    }

    static constexpr int MAX_ATTEMPTS = 3;
    for (int attempts = 1; attempts <= MAX_ATTEMPTS; ++attempts) {
        try {
            task.logger.debug("Signing in...");

            visitClassRegistration(task.sessionManager.getSession()); // Prompts login
            ssbLoginRedirect(task.sessionManager);

            if (!idpSSO(task.sessionManager)) {
                task.logger.debug("Received authentication failure during idpSSO.");
                task.sessionManager.resetSession();
                --attempts;
                continue;
            }

            getLoginPage(task.sessionManager.getSession());
            login(task.sessionManager, task.config.username, task.config.password);
            selfServiceSSO(task.sessionManager);
            visitRegistrationDashboard(task.sessionManager.getSession());
            fetchRegistrationTime(task);

            task.sessionManager.generateUniqueSessionId();
            task.logger.info("Successfully signed in.");
            break;
        } catch (const UnrecoverableException& e) {
            throw UnrecoverableException{fmt::format("Unrecoverable Authentication Error - {}", e.what())};
        } catch (const std::exception& e) {
            task.logger.error("Authentication Error - {}. Attempt {}/{}.", e.what(), attempts, MAX_ATTEMPTS);
            if (attempts == MAX_ATTEMPTS) {
                throw UnrecoverableException{fmt::format("Failed to authenticate after {} attempts.", MAX_ATTEMPTS)};
            }
        }
    }

    task.scheduler.throwIfStopped();
}