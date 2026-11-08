#pragma once

#include <filesystem>

namespace Idler::Cli
{
    struct ServeOptions
    {
        std::filesystem::path Settings;
        bool Start = false;
        bool Pretend = false;
    };

    int Serve(const ServeOptions& options);
}
