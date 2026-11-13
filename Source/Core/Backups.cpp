#include "Core/Backups.h"

#include "Core/Creatures.h"
#include "Core/Errors.h"
#include "Core/Settings.h"
#include "Core/Stats.h"
#include "Core/Text.h"
#include "Core/Zip.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <utility>

namespace Idler::Backups
{
    namespace
    {
        constexpr std::string_view kExtension = ".zip";
        constexpr std::string_view kBeforeRestore = " before restore";
        constexpr std::time_t kNameTries = 60;

        enum class Role
        {
            Settings,
            Stats,
            Cards,
            Other
        };

        std::string Utf8Of(const std::filesystem::path& path)
        {
            const std::u8string text = path.u8string();
            return std::string(text.begin(), text.end());
        }

        std::filesystem::path PathOf(std::string_view utf8)
        {
            return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
        }

        std::optional<std::string> ReadWhole(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return std::nullopt;
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            if (file.bad())
                return std::nullopt;
            return content;
        }

        bool WriteWhole(const std::filesystem::path& path, std::string_view content)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file)
                return false;
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            file.close();
            return !file.fail();
        }

        bool WriteAtomically(const std::filesystem::path& path, std::string_view content)
        {
            std::filesystem::path temporary = path;
            temporary += ".saving";
            std::error_code error;
            if (!WriteWhole(temporary, content))
            {
                std::filesystem::remove(temporary, error);
                return false;
            }
            std::filesystem::rename(temporary, path, error);
            if (error)
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                return false;
            }
            return true;
        }

        std::time_t WrittenAt(const std::filesystem::path& path)
        {
            std::error_code error;
            const auto written = std::filesystem::last_write_time(path, error);
            if (error)
                return 0;
            const auto age = std::filesystem::file_time_type::clock::now() - written;
            return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() - std::chrono::duration_cast<std::chrono::system_clock::duration>(age));
        }

        std::string Stamp(std::time_t time, const char* format)
        {
            std::tm local{};
            if (!LocalTime(time, local))
                return {};
            char text[40];
            const std::size_t length = std::strftime(text, sizeof(text), format, &local);
            return std::string(text, length);
        }

        bool SameDay(std::time_t first, std::time_t second)
        {
            const std::string day = Stamp(first, "%Y-%m-%d");
            return !day.empty() && day == Stamp(second, "%Y-%m-%d");
        }

        bool Digits(std::string_view text, std::size_t at, std::size_t count, int& value)
        {
            value = 0;
            if (at + count > text.size())
                return false;
            for (std::size_t index = at; index < at + count; ++index)
            {
                const char c = text[index];
                if (c < '0' || c > '9')
                    return false;
                value = value * 10 + (c - '0');
            }
            return true;
        }

        Role RoleOf(const std::filesystem::path& settings, std::string_view name)
        {
            const std::string extension = FoldCase(Utf8Of(PathOf(name).filename().extension()));
            if (extension == ".stats")
                return Role::Stats;
            if (extension == ".cards")
                return Role::Cards;
            if (!extension.empty() && extension == FoldCase(Utf8Of(settings.extension())))
                return Role::Settings;
            return Role::Other;
        }

        struct Gathered
        {
            std::vector<Zip::Entry> Entries;
            bool Failed = false;
        };

        Gathered Gather(const std::filesystem::path& settings)
        {
            Gathered gathered;
            for (const std::filesystem::path& file : FilesOf(settings))
            {
                std::error_code error;
                if (!std::filesystem::is_regular_file(file, error))
                    continue;
                std::optional<std::string> data = ReadWhole(file);
                if (!data)
                {
                    gathered.Failed = true;
                    return gathered;
                }
                gathered.Entries.push_back({ Utf8Of(file.filename()), std::move(*data), WrittenAt(file) });
            }
            return gathered;
        }

        std::optional<std::vector<Zip::Entry>> ReadBackup(const std::filesystem::path& file)
        {
            const std::optional<std::string> archive = ReadWhole(file);
            if (!archive)
                return std::nullopt;
            return Zip::Read(*archive);
        }

        bool SameEntries(const std::filesystem::path& backup, const std::vector<Zip::Entry>& entries)
        {
            const std::optional<std::vector<Zip::Entry>> stored = ReadBackup(backup);
            if (!stored || stored->size() != entries.size())
                return false;
            for (const Zip::Entry& entry : entries)
            {
                const auto match = std::find_if(stored->begin(), stored->end(), [&](const Zip::Entry& each) { return each.Name == entry.Name; });
                if (match == stored->end() || match->Data != entry.Data)
                    return false;
            }
            return true;
        }

        std::size_t FoundCreatures(const Settings& settings)
        {
            CreatureSet found;
            for (const CreatureInfo& info : AllCreatures())
            {
                if (info.Kind == UnlockKind::Start)
                    found.set(static_cast<std::size_t>(info.Id));
            }
            for (const StoredCreature& stored : settings.Creatures)
            {
                if (const std::optional<CreatureId> id = CreatureWithKey(stored.Key))
                    found.set(static_cast<std::size_t>(*id));
            }
            return found.count();
        }
    }

    std::filesystem::path FolderOf(const std::filesystem::path& settings)
    {
        std::filesystem::path folder = settings;
        folder.replace_extension(".backups");
        return folder;
    }

    std::vector<std::filesystem::path> FilesOf(const std::filesystem::path& settings)
    {
        std::filesystem::path stats = settings;
        stats.replace_extension(".stats");
        std::filesystem::path cards = settings;
        cards.replace_extension(".cards");
        return { settings, stats, cards };
    }

    std::string FileName(BackupKind kind, std::time_t time)
    {
        switch (kind)
        {
        case BackupKind::Daily:
            return Stamp(time, "%Y-%m-%d") + std::string(kExtension);
        case BackupKind::Saved:
            return Stamp(time, "%Y-%m-%d %H-%M-%S") + std::string(kExtension);
        case BackupKind::BeforeRestore:
            return Stamp(time, "%Y-%m-%d %H-%M-%S") + std::string(kBeforeRestore) + std::string(kExtension);
        }
        return {};
    }

    std::optional<Backup> FromFile(const std::filesystem::path& file)
    {
        const std::string name = Utf8Of(file.filename());
        if (!name.ends_with(kExtension))
            return std::nullopt;
        std::string_view stem(name);
        stem.remove_suffix(kExtension.size());

        int year = 0;
        int month = 0;
        int day = 0;
        if (stem.size() < 10 || !Digits(stem, 0, 4, year) || stem[4] != '-' || !Digits(stem, 5, 2, month) || stem[7] != '-' || !Digits(stem, 8, 2, day))
            return std::nullopt;
        if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31)
            return std::nullopt;

        std::tm local{};
        local.tm_year = year - 1900;
        local.tm_mon = month - 1;
        local.tm_mday = day;
        local.tm_isdst = -1;

        Backup backup;
        backup.File = file;
        if (stem.size() != 10)
        {
            int hour = 0;
            int minute = 0;
            int second = 0;
            if (stem.size() < 19 || stem[10] != ' ' || !Digits(stem, 11, 2, hour) || stem[13] != '-' || !Digits(stem, 14, 2, minute) || stem[16] != '-' ||
                !Digits(stem, 17, 2, second))
                return std::nullopt;
            if (hour > 23 || minute > 59 || second > 60)
                return std::nullopt;
            const std::string_view rest = stem.substr(19);
            if (rest.empty())
                backup.Kind = BackupKind::Saved;
            else if (rest == kBeforeRestore)
                backup.Kind = BackupKind::BeforeRestore;
            else
                return std::nullopt;
            local.tm_hour = hour;
            local.tm_min = minute;
            local.tm_sec = second;
        }

        backup.Time = std::mktime(&local);
        if (backup.Time == static_cast<std::time_t>(-1))
            return std::nullopt;
        return backup;
    }

    std::vector<Backup> List(const std::filesystem::path& settings)
    {
        std::vector<Backup> backups;
        std::error_code error;
        std::filesystem::directory_iterator item(FolderOf(settings), error);
        for (; !error && item != std::filesystem::directory_iterator(); item.increment(error))
        {
            std::error_code ignored;
            if (!item->is_regular_file(ignored))
                continue;
            if (std::optional<Backup> backup = FromFile(item->path()))
            {
                const std::uintmax_t bytes = item->file_size(ignored);
                backup->Bytes = ignored ? 0 : bytes;
                backups.push_back(std::move(*backup));
            }
        }
        std::sort(backups.begin(), backups.end(), [](const Backup& first, const Backup& second)
            {
                if (first.Time != second.Time)
                    return first.Time > second.Time;
                return first.File.filename() > second.File.filename();
            });
        return backups;
    }

    bool MadeToday(const std::filesystem::path& settings, std::time_t now)
    {
        const std::vector<Backup> backups = List(settings);
        return std::any_of(backups.begin(), backups.end(), [&](const Backup& backup) { return backup.Kind == BackupKind::Daily && SameDay(backup.Time, now); });
    }

    Result Make(const std::filesystem::path& settings, BackupKind kind, std::time_t now)
    {
        Result result;
        const std::filesystem::path folder = FolderOf(settings);
        const Gathered gathered = Gather(settings);
        if (gathered.Failed)
        {
            result.Problem = ErrorMessage(ErrorCode::BackupNotMade, Utf8Of(folder));
            return result;
        }
        if (gathered.Entries.empty())
        {
            result.What = Outcome::NothingToBackUp;
            return result;
        }

        const std::vector<Backup> existing = List(settings);
        if (kind == BackupKind::Daily)
        {
            const auto today = std::find_if(existing.begin(), existing.end(), [&](const Backup& backup) { return backup.Kind == BackupKind::Daily && SameDay(backup.Time, now); });
            if (today != existing.end())
            {
                result.What = Outcome::Unchanged;
                result.File = today->File;
                return result;
            }
        }
        const auto compared = kind == BackupKind::Daily
            ? std::find_if(existing.begin(), existing.end(), [](const Backup& backup) { return backup.Kind == BackupKind::Daily; })
            : existing.begin();
        if (compared != existing.end() && SameEntries(compared->File, gathered.Entries))
        {
            result.What = Outcome::Unchanged;
            result.File = compared->File;
            return result;
        }

        const std::optional<std::string> archive = Zip::Write(gathered.Entries);
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        std::filesystem::path target = folder / FileName(kind, now);
        for (std::time_t later = now + 1; std::filesystem::exists(target, error) && later < now + kNameTries; ++later)
            target = folder / FileName(kind, later);
        if (!archive || std::filesystem::exists(target, error) || !WriteAtomically(target, *archive))
        {
            result.Problem = ErrorMessage(ErrorCode::BackupNotMade, Utf8Of(folder));
            return result;
        }

        result.What = Outcome::Made;
        result.File = target;
        Prune(settings);
        return result;
    }

    std::size_t Prune(const std::filesystem::path& settings)
    {
        std::size_t daily = 0;
        std::size_t others = 0;
        std::size_t removed = 0;
        for (const Backup& backup : List(settings))
        {
            const bool keep = backup.Kind == BackupKind::Daily ? ++daily <= kKeepDaily : ++others <= kKeepOthers;
            if (keep)
                continue;
            std::error_code error;
            if (std::filesystem::remove(backup.File, error))
                ++removed;
        }
        return removed;
    }

    std::optional<Contents> Inspect(const std::filesystem::path& settings, const std::filesystem::path& file)
    {
        const std::optional<std::vector<Zip::Entry>> entries = ReadBackup(file);
        if (!entries)
            return std::nullopt;

        Contents contents;
        for (const Zip::Entry& entry : *entries)
        {
            switch (RoleOf(settings, entry.Name))
            {
            case Role::Settings:
            {
                Settings stored;
                stored.Read(entry.Data);
                contents.HasSettings = true;
                contents.Creatures = FoundCreatures(stored);
                break;
            }
            case Role::Stats:
            {
                IdleStats stats;
                stats.Read(entry.Data);
                contents.HasStats = true;
                contents.Seconds = stats.Total();
                break;
            }
            case Role::Cards:
                contents.HasCards = true;
                break;
            case Role::Other:
                break;
            }
        }
        return contents;
    }

    Result Restore(const std::filesystem::path& settings, const std::filesystem::path& backup, std::time_t now)
    {
        Result result;
        const std::optional<std::vector<Zip::Entry>> entries = ReadBackup(backup);
        if (!entries)
        {
            result.Problem = ErrorMessage(ErrorCode::BackupNotRead, Utf8Of(backup.filename()));
            return result;
        }

        const std::vector<std::filesystem::path> files = FilesOf(settings);
        std::array<const std::string*, 3> chosen{};
        for (const Zip::Entry& entry : *entries)
        {
            const Role role = RoleOf(settings, entry.Name);
            if (role == Role::Other)
                continue;
            const auto index = static_cast<std::size_t>(role);
            if (chosen[index])
            {
                result.Problem = ErrorMessage(ErrorCode::BackupNotRead, Utf8Of(backup.filename()));
                return result;
            }
            chosen[index] = &entry.Data;
        }
        if (!chosen[static_cast<std::size_t>(Role::Settings)] && !chosen[static_cast<std::size_t>(Role::Stats)])
        {
            result.Problem = ErrorMessage(ErrorCode::BackupNotRead, Utf8Of(backup.filename()));
            return result;
        }

        const Result before = Make(settings, BackupKind::BeforeRestore, now);
        if (!before.Ok())
            return before;

        std::vector<std::pair<std::filesystem::path, std::filesystem::path>> moves;
        const auto discard = [&]()
            {
                for (const auto& move : moves)
                {
                    std::error_code ignored;
                    std::filesystem::remove(move.first, ignored);
                }
            };
        for (std::size_t index = 0; index < chosen.size(); ++index)
        {
            if (!chosen[index])
                continue;
            std::filesystem::path temporary = files[index];
            temporary += ".restoring";
            moves.emplace_back(temporary, files[index]);
            if (!WriteWhole(temporary, *chosen[index]))
            {
                discard();
                result.Problem = ErrorMessage(ErrorCode::BackupNotRestored, Utf8Of(files[index]));
                return result;
            }
        }
        for (const auto& [temporary, target] : moves)
        {
            std::error_code error;
            std::filesystem::rename(temporary, target, error);
            if (error)
            {
                discard();
                result.Problem = ErrorMessage(ErrorCode::BackupNotRestored, Utf8Of(target));
                return result;
            }
        }

        result.What = Outcome::Made;
        result.File = before.File;
        return result;
    }
}
