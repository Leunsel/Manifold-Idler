#pragma once

#include <filesystem>
#include <string_view>

namespace Idler::CrashHandler
{
    void Install(const std::filesystem::path& settings, const std::filesystem::path& log);
    void CrashOnPurpose(std::wstring_view how);
}
