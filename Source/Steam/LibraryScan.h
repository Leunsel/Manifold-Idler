#pragma once

#include "Core/Library.h"
#include "Steam/SteamLocal.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Idler::Steam
{
    struct ScanResult
    {
        std::vector<Game> Games;
        std::string Persona;
        std::string Problem;
        std::uint32_t Account = 0;
    };

    ScanResult ScanLibrary(const Status& status);
}
