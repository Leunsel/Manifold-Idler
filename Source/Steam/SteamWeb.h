#pragma once

#include <string>
#include <string_view>

namespace Idler::SteamWeb
{
    bool ReadPicture(std::string_view address, std::string& bytes);
}
