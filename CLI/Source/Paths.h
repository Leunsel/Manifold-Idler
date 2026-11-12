#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Idler::Cli
{
    std::filesystem::path PathFromUtf8(std::string_view text);
    std::string PathToUtf8(const std::filesystem::path& path);

    std::filesystem::path DefaultSettingsFile();
    std::filesystem::path OldSettingsFile();
    std::filesystem::path TakeOverOldSettings(const std::filesystem::path& settings);

    std::string SocketName(const std::filesystem::path& settings);
    std::filesystem::path SocketFile(const std::filesystem::path& settings);
    std::filesystem::path WebTokenFile(const std::filesystem::path& settings);
}
