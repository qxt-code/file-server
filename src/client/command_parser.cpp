#include "command_parser.h"

#include <ranges>


json CommandParser::parseCommand(const std::string& input) {
    json command;
    auto tokens = input | std::views::split(' ') | std::views::transform([](auto&& rng) {
        return std::string(&*rng.begin(), std::ranges::distance(rng));
    });
    auto it = tokens.begin();
    if (it != tokens.end()) {
        command["command"] = *it;
        ++it;
    }
    command["params"] = json::array();
    while (it != tokens.end()) {
        std::string token = *it;
        if (!token.empty()) {
            command["params"].push_back(token);
        }
        ++it;
    }
    return command;
}