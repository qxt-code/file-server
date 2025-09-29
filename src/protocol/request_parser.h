#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include "commands.h"
#include "nlohmann/json.hpp"

namespace protocol {

using nlohmann::json;

class RequestParser {
public:
    RequestParser() = default;
    ~RequestParser() = default;
    std::optional<json> parse(const std::string& request);

    const std::string& getCommand() const {
        return command;
    }

    const std::unordered_map<std::string, std::string>& getParameters() const {
        return parameters;
    }

private:
    inline void reset() {
        command.clear();
        parameters.clear();
    }

    std::string command;
    std::unordered_map<std::string, std::string> parameters;
    json requestJson;

};



} // namespace protocol