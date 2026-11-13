#pragma once

#include "Core/Backups.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::Cli
{
    std::string BackupsText(const std::vector<Backups::Backup>& backups, const std::vector<std::string>& contents);
    std::string BackupResultText(const Backups::Result& result);
    std::filesystem::path BackupFileNamed(const std::filesystem::path& settings, std::string_view name);

    int ListBackups(const std::filesystem::path& settings);
    int BackUp(const std::filesystem::path& settings);
    int RestoreBackup(const std::filesystem::path& settings, const std::string& name);
}
