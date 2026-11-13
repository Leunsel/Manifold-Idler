#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Idler::Backups
{
    enum class BackupKind
    {
        Daily,
        Saved,
        BeforeRestore
    };

    inline constexpr std::size_t kKeepDaily = 7;
    inline constexpr std::size_t kKeepOthers = 5;

    struct Backup
    {
        std::filesystem::path File;
        BackupKind Kind = BackupKind::Daily;
        std::time_t Time = 0;
        std::uintmax_t Bytes = 0;
    };

    struct Contents
    {
        bool HasSettings = false;
        bool HasStats = false;
        bool HasCards = false;
        std::size_t Creatures = 0;
        double Seconds = 0.0;
    };

    enum class Outcome
    {
        Made,
        Unchanged,
        NothingToBackUp,
        Failed
    };

    struct Result
    {
        Outcome What = Outcome::Failed;
        std::filesystem::path File;
        std::string Problem;

        bool Ok() const { return What != Outcome::Failed; }
    };

    std::filesystem::path FolderOf(const std::filesystem::path& settings);
    std::vector<std::filesystem::path> FilesOf(const std::filesystem::path& settings);
    std::string FileName(BackupKind kind, std::time_t time);
    std::optional<Backup> FromFile(const std::filesystem::path& file);

    std::vector<Backup> List(const std::filesystem::path& settings);
    bool MadeToday(const std::filesystem::path& settings, std::time_t now);
    Result Make(const std::filesystem::path& settings, BackupKind kind, std::time_t now);
    std::size_t Prune(const std::filesystem::path& settings);

    std::optional<Contents> Inspect(const std::filesystem::path& settings, const std::filesystem::path& file);
    Result Restore(const std::filesystem::path& settings, const std::filesystem::path& backup, std::time_t now);
}
