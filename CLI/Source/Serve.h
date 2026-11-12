#pragma once

#include <filesystem>
#include <string>

namespace Idler::Cli
{
    struct ServeOptions
    {
        std::filesystem::path Settings;
        bool Start = false;
        bool Pretend = false;
        bool Web = false;
        std::string WebAddress;
    };

    int Serve(const ServeOptions& options);
}
