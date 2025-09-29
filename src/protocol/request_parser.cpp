#include "request_parser.h"
#include "commands.h"
#include "error_codes.h"
#include <string>
#include <sstream>

namespace protocol {


std::optional<json> RequestParser::parse(const std::string& request) {
    reset();
    // try {
        auto j = json::parse(request);
        if (j.contains("command") && j["command"].is_string()) {
            command = j["command"].get<std::string>();
        } else {
            return std::nullopt;
        }

        if (j.contains("parameters") && j["parameters"].is_object()) {
            for (auto& [key, value] : j["parameters"].items()) {
                if (value.is_string()) {
                    parameters[key] = value.get<std::string>();
                } else {
                    parameters[key] = value.dump();
                }
            }
        }
        return j;
    // } catch (const json::parse_error&) {
    //     return std::nullopt;
    // }
}



} // namespace protocol