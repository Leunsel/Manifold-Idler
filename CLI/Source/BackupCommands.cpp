#include "BackupCommands.h"

#include "Core/Errors.h"
#include "Core/FileLock.h"
#include "Core/Text.h"
#include "Paths.h"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <system_error>

namespace Idler::Cli
{
    namespace
    {
        const char* KindText(Backups::BackupKind kind)
        {
            switch (kind)
            {
            case Backups::BackupKind::Daily:
                return "daily";
            case Backups::BackupKind::Saved:
                return "by hand";
            case Backups::BackupKind::BeforeRestore:
                return "before a restore";
            }
            return "";
        }

        std::string ContentsText(const std::optional<Backups::Contents>& contents)
        {
            if (!contents)
                return "cannot be read";
            std::string text;
            if (contents->HasSettings)
                text += std::to_string(contents->Creatures) + (contents->Creatures == 1 ? " creature" : " creatures");
            if (contents->HasStats)
                text += (text.empty() ? "" : ", ") + ShortDurationText(static_cast<std::uint64_t>(contents->Seconds)) + " idled";
            return text;
        }

        std::string Padded(std::string text, std::size_t width)
        {
            if (text.size() < width)
                text.append(width - text.size(), ' ');
            return text;
        }
    }

    std::string BackupsText(const std::vector<Backups::Backup>& backups, const std::vector<std::string>& contents)
    {
        if (backups.empty())
            return "No backups yet.\n";
        std::size_t nameWidth = 0;
        std::size_t kindWidth = 0;
        for (const Backups::Backup& backup : backups)
        {
            nameWidth = std::max(nameWidth, PathToUtf8(backup.File.filename()).size());
            kindWidth = std::max(kindWidth, std::string(KindText(backup.Kind)).size());
        }
        std::string text;
        for (std::size_t index = 0; index < backups.size(); ++index)
        {
            const Backups::Backup& backup = backups[index];
            std::string line = Padded(PathToUtf8(backup.File.filename()), nameWidth + 2) + Padded(KindText(backup.Kind), kindWidth + 2);
            if (index < contents.size())
                line += contents[index];
            while (!line.empty() && line.back() == ' ')
                line.pop_back();
            text += line + "\n";
        }
        return text;
    }

    std::string BackupResultText(const Backups::Result& result)
    {
        switch (result.What)
        {
        case Backups::Outcome::Made:
            return "Backed up as " + PathToUtf8(result.File.filename()) + ".";
        case Backups::Outcome::Unchanged:
            return "Nothing changed since the last backup, " + PathToUtf8(result.File.filename()) + ".";
        case Backups::Outcome::NothingToBackUp:
            return "Nothing is saved yet that could be backed up.";
        case Backups::Outcome::Failed:
            return result.Problem;
        }
        return {};
    }

    std::filesystem::path BackupFileNamed(const std::filesystem::path& settings, std::string_view name)
    {
        const std::filesystem::path given = PathFromUtf8(name);
        std::error_code error;
        if (given.has_parent_path() && std::filesystem::is_regular_file(given, error))
            return given;
        const std::filesystem::path folder = Backups::FolderOf(settings);
        if (std::filesystem::is_regular_file(folder / given, error) || name.ends_with(".zip"))
            return folder / given;
        std::filesystem::path zipped = given;
        zipped += ".zip";
        return folder / zipped;
    }

    int ListBackups(const std::filesystem::path& settings)
    {
        const std::vector<Backups::Backup> backups = Backups::List(settings);
        std::vector<std::string> contents;
        for (const Backups::Backup& backup : backups)
            contents.push_back(ContentsText(Backups::Inspect(settings, backup.File)));
        std::fputs(BackupsText(backups, contents).c_str(), stdout);
        std::printf("Folder: %s\n", PathToUtf8(Backups::FolderOf(settings)).c_str());
        return 0;
    }

    int BackUp(const std::filesystem::path& settings)
    {
        const Backups::Result result = Backups::Make(settings, Backups::BackupKind::Saved, std::time(nullptr));
        std::fprintf(result.Ok() ? stdout : stderr, "%s\n", BackupResultText(result).c_str());
        return result.Ok() ? 0 : 1;
    }

    int RestoreBackup(const std::filesystem::path& settings, const std::string& name)
    {
        FileLock lock;
        if (!lock.Take(SettingsLockFile(settings)))
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::SettingsInUse).c_str());
            return 1;
        }
        const std::filesystem::path file = BackupFileNamed(settings, name);
        const Backups::Result result = Backups::Restore(settings, file, std::time(nullptr));
        if (!result.Ok())
        {
            std::fprintf(stderr, "%s\n", result.Problem.c_str());
            return 1;
        }
        std::printf("Restored %s.", PathToUtf8(file.filename()).c_str());
        if (!result.File.empty())
            std::printf(" What was there before is in %s.", PathToUtf8(result.File.filename()).c_str());
        std::printf("\n");
        return 0;
    }
}
