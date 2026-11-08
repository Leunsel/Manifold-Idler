#pragma once

#include "Core/Queue.h"

#include <string>
#include <string_view>
#include <vector>

namespace Idler::Cli
{
    std::string JoinRequest(const std::vector<std::string>& words);
    std::vector<std::string> SplitRequest(std::string_view line);

    std::string MakeReply(bool ok, std::string_view text);
    bool ReadReply(std::string_view reply, bool& ok, std::string& text);

    bool ParseGoal(const std::vector<std::string>& words, std::size_t first, Goal& goal, std::string& problem);
    bool ParseHours(std::string_view text, float& hours);
}
