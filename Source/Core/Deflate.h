#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace Idler::Deflate
{
    std::optional<std::string> Pack(std::string_view data);
    std::optional<std::string> Unpack(std::string_view packed, std::size_t size);
}
