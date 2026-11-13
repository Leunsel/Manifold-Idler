#pragma once

#include <cstddef>
#include <string>

namespace Idler::ReleaseCheck
{
    inline constexpr std::size_t kMostBytes = 1024u * 1024u;

    bool FetchLatest(std::string& body, std::string& problem);
}
