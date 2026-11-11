#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Idler::Cli
{
    std::string QuoteForUnit(std::string_view text);
    std::string ServiceName(const std::filesystem::path& settings, const std::filesystem::path& defaultSettings);
    std::string ServiceUnit(const std::filesystem::path& program, const std::filesystem::path& settings, bool start, std::string_view web = {});

    int InstallService(const std::filesystem::path& settings, bool start, bool web, const std::string& webAddress);
    int RemoveService(const std::filesystem::path& settings);
}
