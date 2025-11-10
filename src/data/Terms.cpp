#include "data/Terms.h"
#include "Links.h"
#include "util/Requests.h"
#include "util/Utility.h"

#include <rapidjson/document.h>
#include <spdlog/spdlog.h>

#include <vector>

namespace {
std::unordered_map<std::string, std::string> getTerms() {
    cpr::Session session;
    const std::string responseText = sendRequest(session, RequestMethod::GET, Link::Terms::TERMS,
        cpr::Parameters{
            {"searchTerm", ""},
            {"offset", "1"},
            {"max", "4"},
        }
    ).text;

    std::unordered_map<std::string, std::string> terms;
    terms.reserve(4);

    for (const rapidjson::Document document = parseJsonResponse(responseText);
            const auto& term : document.GetArray()) {
        std::string code = term["code"].GetString();
        std::string description = term["description"].GetString();

        static constexpr std::string_view VIEW_ONLY_SUFFIX = " (View Only)";
        if (description.ends_with(VIEW_ONLY_SUFFIX)) {
            description.erase(description.size() - VIEW_ONLY_SUFFIX.size(), VIEW_ONLY_SUFFIX.size());
        }

        terms.insert({std::move(description), std::move(code)});
    }

    return terms;
}

// Converts the term from "YYYY Season Campus" to "YYYYSC"
// Season: Summer -> 1, Fall -> 2, Winter -> 3, Spring -> 4 (If Summer or Fall, then the year is incremented by 1)
// Campus: Foothill -> 1, De Anza -> 2
std::string buildTerm(const std::string& termDescription) {
    const std::vector<std::string> splitTerm = split(termDescription, " ");

    std::string termCode = splitTerm[0]; // Year

    const std::string_view season = splitTerm[1];
    const std::string_view school = splitTerm[2];

    if (season == "Summer") {
        // Summer and Fall will take the next year (so in 2025-2026, they will take 2026).
        ++termCode[3];
        termCode += '1';
    } else if (season == "Fall") {
        ++termCode[3];
        termCode += '2';
    } else if (season == "Winter") {
        termCode += '3';
    } else if (season == "Spring") {
        termCode += '4';
    } else {
        throw std::runtime_error{"Invalid term: " + termDescription};
    }

    switch (school.front()) {
        case 'F':
            termCode += '1';
            break;
        case 'D':
            termCode += '2';
            break;
        default:
            throw std::runtime_error{"Invalid term: " + termDescription};
    }

    return termCode;
}
} // namespace

std::string getTermCode(const std::string& termDescription) {
    static std::unordered_map<std::string, std::string> terms;

    static std::once_flag termsFlag;
    std::call_once(termsFlag, [] {
        terms = getTerms();
    });

    if (!terms.contains(termDescription)) {
        if (terms.empty()) {
            spdlog::get("console")->warn("Could not get terms from server. Manually building term code.");
            return buildTerm(termDescription);
        }

        throw std::runtime_error{"Invalid or out-of-date term in configuration file: " + termDescription};
    }

    return terms.at(termDescription);
}