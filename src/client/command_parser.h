#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include "types/message.h"

using nlohmann::json;

class CommandParser
{
public:
    json parseCommand(const std::string& input);
};