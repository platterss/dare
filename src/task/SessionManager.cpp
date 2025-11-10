#include "task/SessionManager.h"
#include "util/Requests.h"

#include <random>

SessionManager::SessionManager() {
    resetSession();
}

cpr::Session& SessionManager::getSession() const noexcept {
    return *m_session;
}

void SessionManager::resetSession() {
    m_session = std::make_unique<cpr::Session>();
    m_session->SetRedirect(cpr::Redirect{false});
    m_session->SetHeader(getDefaultHeaders());
    generateUniqueSessionId();
}

void SessionManager::generateUniqueSessionId() {
    static constexpr std::size_t UNIQUE_SESSION_ID_SIZE = 18;
    static constexpr std::string_view LETTERS = "abcdefghijklmnopqrstuvwxyz";
    static constexpr std::string_view DIGITS = "0123456789";

    // The unique session ID is an 18-character string, where:
    // - the first 5 characters are lowercase letters, with one character sometimes being a digit, and
    // - the last 13 characters are the UNIX timestamp in milliseconds.
    // Not really sure if the first five characters are supposed to mean anything.
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> letterDist{0, LETTERS.size() - 1};
    std::uniform_int_distribution<> digitDist{0, DIGITS.size() - 1};
    std::uniform_int_distribution digitIndex{0, 4};
    std::bernoulli_distribution includeDigit{0.5};

    uniqueSessionId.clear();
    uniqueSessionId.reserve(UNIQUE_SESSION_ID_SIZE);

    const int indexDigit = digitIndex(gen);
    for (int i = 0; i < 5; ++i) {
        if (i == indexDigit && includeDigit(gen)) {
            uniqueSessionId.push_back(DIGITS[digitDist(gen)]);
            continue;
        }

        uniqueSessionId.push_back(LETTERS[letterDist(gen)]);
    }

    const auto currentUnixTimeMs = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    uniqueSessionId.append(currentUnixTimeMs);
}