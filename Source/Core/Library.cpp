#include "Core/Library.h"

#include "Core/Text.h"
#include "Core/Vdf.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <charconv>
#include <climits>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <tuple>
#include <utility>

namespace Idler
{
    namespace
    {
#ifndef _WIN32
        bool NameBefore(const Game& lhs, const Game& rhs)
        {
            const std::string a = FoldCase(lhs.Name);
            const std::string b = FoldCase(rhs.Name);
            const auto digit = [](char character) { return character >= '0' && character <= '9'; };
            std::size_t i = 0;
            std::size_t j = 0;
            while (i < a.size() && j < b.size())
            {
                if (!digit(a[i]) || !digit(b[j]))
                {
                    if (a[i] != b[j])
                        return static_cast<unsigned char>(a[i]) < static_cast<unsigned char>(b[j]);
                    ++i;
                    ++j;
                    continue;
                }
                while (i < a.size() && a[i] == '0')
                    ++i;
                while (j < b.size() && b[j] == '0')
                    ++j;
                std::size_t endA = i;
                std::size_t endB = j;
                while (endA < a.size() && digit(a[endA]))
                    ++endA;
                while (endB < b.size() && digit(b[endB]))
                    ++endB;
                if (endA - i != endB - j)
                    return endA - i < endB - j;
                const int order = a.compare(i, endA - i, b, j, endB - j);
                if (order != 0)
                    return order < 0;
                i = endA;
                j = endB;
            }
            return a.size() - i < b.size() - j;
        }
#else
        bool NameBefore(const Game& lhs, const Game& rhs)
        {
            const std::wstring a = Utf8ToWide(lhs.Name);
            const std::wstring b = Utf8ToWide(rhs.Name);
            return ::CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS, a.c_str(), static_cast<int>(a.size()), b.c_str(),
                static_cast<int>(b.size()), nullptr, nullptr, 0) == CSTR_LESS_THAN;
        }
#endif

        void ReadRegistryNode(const Vdf::Node& node, SteamRegistry& registry)
        {
            if (EqualsIgnoreCase(node.Key, "ActiveProcess"))
            {
                const auto number = [&](std::string_view key) -> std::uint32_t
                    {
                        const Vdf::Node* value = node.Find(key);
                        return value ? ParseUnsigned(Trim(value->Value)).value_or(0) : 0;
                    };
                registry.ProcessId = number("pid");
                registry.Account = number("ActiveUser");
                if (const Vdf::Node* library = node.Find("SteamClientDll64"))
                    registry.ClientLibrary = library->Value;
            }
            else if (EqualsIgnoreCase(node.Key, "Apps"))
            {
                for (const Vdf::Node& app : node.Children)
                {
                    const Vdf::Node* running = app.Find("Running");
                    const auto appId = ParseUnsigned(Trim(app.Key));
                    if (running && appId && ParseUnsigned(Trim(running->Value)).value_or(0) != 0)
                        registry.RunningApps.push_back(*appId);
                }
            }
            for (const Vdf::Node& child : node.Children)
                ReadRegistryNode(child, registry);
        }
    }

    SteamRegistry ParseSteamRegistry(std::string_view text)
    {
        Vdf::Node root;
        Vdf::Parse(text, root);
        SteamRegistry registry;
        ReadRegistryNode(root, registry);
        return registry;
    }

    AchievementCount ParseAchievementCounts(std::string_view json)
    {
        const auto number = [&](std::string_view key) -> std::uint32_t
            {
                const auto found = json.find(key);
                if (found == std::string_view::npos)
                    return 0;
                std::size_t start = found + key.size();
                std::size_t end = start;
                while (end < json.size() && json[end] >= '0' && json[end] <= '9')
                    ++end;
                return ParseUnsigned(json.substr(start, end - start)).value_or(0);
            };
        AchievementCount count;
        count.Total = number("\"nTotal\":");
        count.Done = count.Total > 0 ? number("\"nAchieved\":") : 0;
        return count;
    }

    bool IsLibraryType(std::string_view type)
    {
        return type == "game" || type == "application";
    }

    PlaytimeMap ParsePlaytimes(std::string_view localConfig)
    {
        PlaytimeMap playtimes;
        Vdf::Node root;
        if (!Vdf::Parse(localConfig, root))
            return playtimes;

        const Vdf::Node* node = &root;
        for (const std::string_view key : { "UserLocalConfigStore", "Software", "Valve", "Steam", "apps" })
        {
            node = node->Find(key);
            if (!node)
                return playtimes;
        }

        for (const Vdf::Node& app : node->Children)
        {
            const auto appId = ParseUnsigned(app.Key);
            if (!appId)
                continue;
            Playtime playtime;
            if (const Vdf::Node* minutes = app.Find("Playtime"))
                playtime.Minutes = ParseUnsigned(minutes->Value).value_or(0);
            if (const Vdf::Node* last = app.Find("LastPlayed"))
                playtime.LastPlayed = ParseUnsigned(last->Value).value_or(0);
            if (playtime.Minutes > 0 || playtime.LastPlayed > 0)
                playtimes[*appId] = playtime;
        }
        return playtimes;
    }

    bool MarkGamesWithCards(std::vector<Game>& games, const std::unordered_map<std::uint32_t, std::uint32_t>& drops)
    {
        bool changed = false;
        for (Game& game : games)
        {
            if (!game.HasCards && drops.contains(game.AppId))
            {
                game.HasCards = true;
                changed = true;
            }
        }
        return changed;
    }

    std::vector<Game> BuildLibrary(const std::vector<AppInfoEntry>& apps, const std::vector<Ownership>& owned, const PlaytimeMap& playtimes,
        std::uint32_t freePackageDate, const AchievementMap& achievements)
    {
        std::unordered_map<std::uint32_t, const AppInfoEntry*> known;
        known.reserve(apps.size());
        for (const AppInfoEntry& app : apps)
            known[app.AppId] = &app;

        std::vector<Game> games;
        games.reserve(owned.size());
        for (const Ownership& own : owned)
        {
            const auto entry = known.find(own.AppId);
            if (entry == known.end() || !IsLibraryType(entry->second->Type) || entry->second->Name.empty())
                continue;

            Playtime playtime;
            if (const auto found = playtimes.find(own.AppId); found != playtimes.end())
                playtime = found->second;

            const bool freePackage = freePackageDate != 0 && own.Purchased == freePackageDate;
            if (freePackage && !own.Installed && playtime.Minutes == 0)
                continue;

            Game game;
            game.AppId = own.AppId;
            game.Name = entry->second->Name;
            game.Folded = FoldCase(game.Name);
            game.HasCards = entry->second->HasCards;
            game.HasAchievements = entry->second->HasAchievements;
            game.Installed = own.Installed;
            game.Software = entry->second->Type == "application";
            game.PlaytimeMinutes = playtime.Minutes;
            game.LastPlayed = playtime.LastPlayed;
            game.Purchased = freePackage ? 0 : own.Purchased;
            game.ReviewPercent = entry->second->ReviewPercent;
            game.Metacritic = entry->second->Metacritic;
            game.Released = entry->second->Released;
            if (const auto found = achievements.find(own.AppId); found != achievements.end())
            {
                game.AchievementsDone = found->second.Done;
                game.AchievementsTotal = found->second.Total;
            }
            games.push_back(std::move(game));
        }

        std::sort(games.begin(), games.end(), NameBefore);
        return games;
    }

    const char* FilterName(LibraryFilter filter)
    {
        switch (filter)
        {
        case LibraryFilter::Cards: return "With cards";
        case LibraryFilter::Installed: return "Installed";
        case LibraryFilter::Played: return "Played";
        case LibraryFilter::NotPlayed: return "Not played";
        default: return "All games";
        }
    }

    const char* SortName(LibrarySort sort)
    {
        switch (sort)
        {
        case LibrarySort::Playtime: return "Playtime";
        case LibrarySort::LastPlayed: return "Last played";
        case LibrarySort::Purchased: return "Purchased";
        case LibrarySort::Reviews: return "Reviews";
        case LibrarySort::Metacritic: return "Metacritic";
        case LibrarySort::Released: return "Released";
        case LibrarySort::Achievements: return "Achievements";
        default: return "Name";
        }
    }

    namespace
    {
        std::string_view JsonStringField(std::string_view json, std::string_view field)
        {
            const std::string key = "\"" + std::string(field) + "\"";
            auto at = json.find(key);
            if (at == std::string_view::npos)
                return {};
            at += key.size();
            while (at < json.size() && (json[at] == ':' || json[at] == ' ' || json[at] == '\t' || json[at] == '\r' || json[at] == '\n'))
                ++at;
            if (at >= json.size() || json[at] != '"')
                return {};
            std::size_t end = at + 1;
            while (end < json.size() && json[end] != '"')
                end += json[end] == '\\' ? 2 : 1;
            return end < json.size() ? json.substr(at, end - at + 1) : std::string_view{};
        }

        std::vector<std::uint32_t> JsonNumbersField(std::string_view json, std::string_view field)
        {
            std::vector<std::uint32_t> numbers;
            const std::string key = "\"" + std::string(field) + "\"";
            auto at = json.find(key);
            if (at == std::string_view::npos)
                return numbers;
            at = json.find('[', at + key.size());
            if (at == std::string_view::npos)
                return numbers;
            std::uint64_t number = 0;
            bool inNumber = false;
            for (++at; at < json.size() && json[at] != ']'; ++at)
            {
                const char next = json[at];
                if (next >= '0' && next <= '9')
                {
                    number = number * 10 + static_cast<std::uint64_t>(next - '0');
                    inNumber = true;
                    continue;
                }
                if (inNumber && number <= 0xFFFFFFFFull)
                    numbers.push_back(static_cast<std::uint32_t>(number));
                number = 0;
                inNumber = false;
            }
            if (inNumber && number <= 0xFFFFFFFFull)
                numbers.push_back(static_cast<std::uint32_t>(number));
            return numbers;
        }

        std::size_t JsonObjectEnd(std::string_view json, std::size_t start)
        {
            int depth = 0;
            for (std::size_t at = start; at < json.size(); ++at)
            {
                if (json[at] == '"')
                {
                    for (++at; at < json.size() && json[at] != '"'; ++at)
                    {
                        if (json[at] == '\\')
                            ++at;
                    }
                }
                else if (json[at] == '{')
                {
                    ++depth;
                }
                else if (json[at] == '}' && --depth == 0)
                {
                    return at + 1;
                }
            }
            return std::string_view::npos;
        }
    }

    namespace
    {
        void CollectAchievementIcons(const Vdf::Node& node, std::vector<AchievementIcons>& icons)
        {
            const Vdf::Node* name = node.Find("name");
            const Vdf::Node* display = node.Find("display");
            const Vdf::Node* icon = display ? display->Find("icon") : nullptr;
            if (name && icon && !name->Value.empty())
            {
                const Vdf::Node* locked = display->Find("icon_gray");
                const Vdf::Node* permission = node.Find("permission");
                const std::optional<std::uint32_t> who = permission ? ParseUnsigned(permission->Value) : std::optional<std::uint32_t>(0);
                AchievementIcons entry;
                entry.Id = name->Value;
                entry.Icon = icon->Value;
                entry.IconLocked = locked ? locked->Value : std::string();
                entry.Permission = who.value_or(3);
                if (const Vdf::Node* progress = node.Find("progress"))
                {
                    const Vdf::Node* value = progress->Find("value");
                    const Vdf::Node* stat = value ? value->Find("operand1") : nullptr;
                    const Vdf::Node* low = progress->Find("min_val");
                    const Vdf::Node* high = progress->Find("max_val");
                    const double from = low ? ParseNumber(low->Value).value_or(0.0) : 0.0;
                    const double to = high ? ParseNumber(high->Value).value_or(from) : from;
                    if (stat && !stat->Value.empty() && to > from)
                    {
                        entry.ProgressStat = stat->Value;
                        entry.ProgressMin = from;
                        entry.ProgressMax = to;
                    }
                }
                icons.push_back(std::move(entry));
                return;
            }
            for (const Vdf::Node& child : node.Children)
                CollectAchievementIcons(child, icons);
        }
    }

    std::vector<AchievementIcons> ParseAchievementIcons(std::string_view schema)
    {
        Vdf::Node root;
        Vdf::ParseBinary(schema, root);
        std::vector<AchievementIcons> icons;
        CollectAchievementIcons(root, icons);
        return icons;
    }

    bool MaySetAchievement(std::string_view id, const std::vector<AchievementIcons>& schema)
    {
        const auto known = std::find_if(schema.begin(), schema.end(), [&](const AchievementIcons& entry) { return entry.Id == id; });
        return known != schema.end() && (known->Permission & 3u) == 0;
    }

    UnlockRule UnlockRuleOf(const std::vector<AchievementIcons>& schema)
    {
        if (schema.empty())
            return UnlockRule::Unknown;
        const auto here = std::count_if(schema.begin(), schema.end(), [](const AchievementIcons& entry) { return (entry.Permission & 3u) == 0; });
        if (here == 0)
            return UnlockRule::Server;
        return static_cast<std::size_t>(here) == schema.size() ? UnlockRule::Here : UnlockRule::Mixed;
    }

    std::optional<double> ParseNumber(std::string_view text)
    {
        text = Trim(text);
        if (text.empty())
            return std::nullopt;
        std::istringstream in{ std::string(text) };
        in.imbue(std::locale::classic());
        double value = 0.0;
        in >> value;
        if (in.fail() || !std::isfinite(value))
            return std::nullopt;
        char rest = 0;
        if (in >> rest)
            return std::nullopt;
        return value;
    }

    std::string NumberText(double value, StatKind kind)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        if (kind == StatKind::Int)
        {
            out << static_cast<long long>(std::llround(value));
            return out.str();
        }
        char digits[32];
        const std::to_chars_result written = std::to_chars(digits, digits + sizeof(digits), static_cast<float>(value));
        return written.ec == std::errc() ? std::string(digits, written.ptr) : std::string("0");
    }

    namespace
    {
        std::optional<StatKind> KindOfStat(const Vdf::Node& stat)
        {
            const Vdf::Node* type = stat.Find("type_int");
            if (!type)
                type = stat.Find("type");
            if (!type)
                return std::nullopt;
            std::string name = FoldCase(type->Value);
            if (name == "1" || name == "int")
                return StatKind::Int;
            if (name == "2" || name == "float")
                return StatKind::Float;
            if (name == "3" || name == "avgrate")
                return StatKind::AverageRate;
            return std::nullopt;
        }

        std::string LabelOf(const Vdf::Node* display)
        {
            const Vdf::Node* name = display ? display->Find("name") : nullptr;
            if (!name)
                return {};
            if (name->Children.empty())
                return name->Value;
            if (const Vdf::Node* english = name->Find("english"))
                return english->Value;
            return name->Children.front().Value;
        }
    }

    std::vector<StatSchema> ParseStatSchema(std::string_view schema)
    {
        Vdf::Node root;
        Vdf::ParseBinary(schema, root);
        std::vector<StatSchema> stats;
        for (const Vdf::Node& game : root.Children)
        {
            const Vdf::Node* list = game.Find("stats");
            if (!list)
                continue;
            for (const Vdf::Node& entry : list->Children)
            {
                const std::optional<StatKind> kind = KindOfStat(entry);
                const Vdf::Node* name = entry.Find("name");
                if (!kind || !name || name->Value.empty())
                    continue;
                StatSchema stat;
                stat.Name = name->Value;
                stat.Label = LabelOf(entry.Find("display"));
                stat.Kind = *kind;
                const Vdf::Node* permission = entry.Find("permission");
                stat.Permission = permission ? ParseUnsigned(permission->Value).value_or(3) : 0;
                const Vdf::Node* increment = entry.Find("incrementonly");
                stat.IncrementOnly = increment && (increment->Value == "1" || FoldCase(increment->Value) == "true");
                const auto number = [&](const char* key) -> std::optional<double>
                    {
                        const Vdf::Node* node = entry.Find(key);
                        return node ? ParseNumber(node->Value) : std::nullopt;
                    };
                stat.Min = number("min");
                stat.Max = number("max");
                if (stat.Min && stat.Max && *stat.Max <= *stat.Min)
                    stat.Min = stat.Max = std::nullopt;
                stat.MaxChange = number("maxchange");
                if (stat.MaxChange && *stat.MaxChange <= 0.0)
                    stat.MaxChange = std::nullopt;
                stats.push_back(std::move(stat));
            }
        }
        return stats;
    }

    const StatSchema* FindStat(const std::vector<StatSchema>& stats, std::string_view name)
    {
        const auto found = std::find_if(stats.begin(), stats.end(), [&](const StatSchema& stat) { return stat.Name == name; });
        return found != stats.end() ? &*found : nullptr;
    }

    bool MaySetStat(const StatSchema& stat)
    {
        return (stat.Permission & 3u) == 0 && stat.Kind != StatKind::AverageRate;
    }

    std::string StatChangeProblem(const StatSchema& stat, double current, double value)
    {
        if (stat.Kind == StatKind::AverageRate)
            return "The game works this one out by itself.";
        if ((stat.Permission & 3u) != 0)
            return "Only the game's own server can set this one.";
        if (!std::isfinite(value))
            return "That is no number.";
        if (stat.Kind == StatKind::Int && value != std::floor(value))
            return "Only whole numbers.";
        if (stat.Kind == StatKind::Int && (value < static_cast<double>(INT_MIN) || value > static_cast<double>(INT_MAX)))
            return "That is too large for it.";
        if (stat.Min && value < *stat.Min)
            return "Not below " + NumberText(*stat.Min, stat.Kind) + ".";
        if (stat.Max && value > *stat.Max)
            return "Not above " + NumberText(*stat.Max, stat.Kind) + ".";
        if (stat.IncrementOnly && value < current)
            return "It only counts up, so not below " + NumberText(current, stat.Kind) + ".";
        if (stat.MaxChange && std::fabs(value - current) > *stat.MaxChange)
            return "At most " + NumberText(*stat.MaxChange, stat.Kind) + " away from " + NumberText(current, stat.Kind) + " at a time.";
        return {};
    }

    std::string WriteAchievementChanges(const std::vector<AchievementChange>& changes)
    {
        std::string text;
        for (const AchievementChange& change : changes)
        {
            if (change.Id.empty() || change.Id.find_first_of("\r\n") != std::string::npos)
                continue;
            if (change.SetsStat)
            {
                std::ostringstream value;
                value.imbue(std::locale::classic());
                value << std::setprecision(17) << change.Value;
                text += "set " + value.str() + " ";
            }
            else
                text += change.Unlock ? "unlock " : "lock ";
            text += change.Id;
            text += '\n';
        }
        return text;
    }

    std::vector<AchievementChange> ParseAchievementChanges(std::string_view text)
    {
        std::vector<AchievementChange> changes;
        std::size_t start = 0;
        while (start < text.size())
        {
            std::size_t end = text.find('\n', start);
            if (end == std::string_view::npos)
                end = text.size();
            std::string_view line = text.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.remove_suffix(1);
            start = end + 1;
            if (line.starts_with("set "))
            {
                const std::string_view rest = line.substr(4);
                const std::size_t space = rest.find(' ');
                const std::optional<double> value = space == std::string_view::npos ? std::nullopt : ParseNumber(rest.substr(0, space));
                if (value && space + 1 < rest.size())
                    changes.push_back({ std::string(rest.substr(space + 1)), false, true, *value });
                continue;
            }
            for (const auto& [word, unlock] : { std::pair<std::string_view, bool>("unlock ", true), std::pair<std::string_view, bool>("lock ", false) })
            {
                if (line.starts_with(word) && line.size() > word.size())
                {
                    changes.push_back({ std::string(line.substr(word.size())), unlock });
                    break;
                }
            }
        }
        return changes;
    }

    std::string AchievementChangeText(std::size_t unlocked, std::size_t locked, std::size_t refused)
    {
        std::string text;
        const auto add = [&](const char* what, std::size_t count)
            {
                if (count == 0)
                    return;
                text += text.empty() ? "" : ", ";
                text += what;
                text += std::to_string(count);
            };
        add("unlocked ", unlocked);
        add("locked ", locked);
        add("Steam refused ", refused);
        if (text.empty())
            return text;
        if (text[0] >= 'a' && text[0] <= 'z')
            text[0] = static_cast<char>(text[0] - 'a' + 'A');
        return text + ".";
    }

    std::vector<SteamCollection> ParseSteamCollections(std::string_view json)
    {
        constexpr std::string_view kPrefix = "\"user-collections.";
        std::vector<SteamCollection> collections;
        std::size_t at = 0;
        while ((at = json.find(kPrefix, at)) != std::string_view::npos)
        {
            const std::size_t start = json.find('{', at);
            const std::size_t end = start == std::string_view::npos ? std::string_view::npos : JsonObjectEnd(json, start);
            if (end == std::string_view::npos)
                break;
            at = end;
            const std::string value = DecodeJsonString(JsonStringField(json.substr(start, end - start), "value"));
            if (value.empty())
                continue;

            SteamCollection collection;
            collection.Name = DecodeJsonString(JsonStringField(value, "name"));
            collection.Apps = JsonNumbersField(value, "added");
            for (const std::uint32_t removed : JsonNumbersField(value, "removed"))
                std::erase(collection.Apps, removed);
            if (!collection.Name.empty() && !collection.Apps.empty())
                collections.push_back(std::move(collection));
        }
        std::stable_sort(collections.begin(), collections.end(), [](const SteamCollection& lhs, const SteamCollection& rhs) { return FoldCase(lhs.Name) < FoldCase(rhs.Name); });
        return collections;
    }

    void ClickSelection(std::unordered_set<std::uint32_t>& selected, std::uint32_t& current, std::uint32_t& anchor, const std::vector<std::uint32_t>& shown, std::uint32_t appId, bool ctrl, bool shift)
    {
        const auto from = std::find(shown.begin(), shown.end(), anchor);
        const auto to = std::find(shown.begin(), shown.end(), appId);
        if (shift && selected.contains(anchor) && from != shown.end() && to != shown.end())
        {
            selected.clear();
            selected.insert(std::min(from, to), std::max(from, to) + 1);
            current = appId;
            return;
        }
        anchor = appId;
        if (ctrl)
        {
            if (selected.erase(appId) == 0)
                selected.insert(appId);
            current = selected.contains(appId) ? appId : selected.empty() ? 0u : *selected.begin();
            return;
        }
        selected = { appId };
        current = appId;
    }

    bool MatchesSearch(std::string_view foldedName, std::uint32_t appId, std::string_view needle)
    {
        return foldedName.find(needle) != std::string_view::npos || std::to_string(appId) == needle;
    }

    std::vector<int> LibraryView(const std::vector<Game>& games, std::string_view search, LibraryFilter filter, LibrarySort sort, std::uint32_t minReview)
    {
        const std::string needle = FoldCase(Trim(search));
        std::vector<int> view;
        view.reserve(games.size());
        for (std::size_t index = 0; index < games.size(); ++index)
        {
            const Game& game = games[index];
            const bool shown = filter == LibraryFilter::All
                || (filter == LibraryFilter::Cards && game.HasCards)
                || (filter == LibraryFilter::Installed && game.Installed)
                || (filter == LibraryFilter::Played && game.PlaytimeMinutes > 0)
                || (filter == LibraryFilter::NotPlayed && game.PlaytimeMinutes == 0);
            if (!shown || game.ReviewPercent < minReview)
                continue;
            if (!needle.empty() && !MatchesSearch(game.Folded, game.AppId, needle))
                continue;
            view.push_back(static_cast<int>(index));
        }

        const auto by = [&](auto value)
            {
                std::stable_sort(view.begin(), view.end(), [&](int lhs, int rhs)
                    { return value(games[static_cast<std::size_t>(lhs)]) > value(games[static_cast<std::size_t>(rhs)]); });
            };
        switch (sort)
        {
        case LibrarySort::Playtime: by([](const Game& game) { return game.PlaytimeMinutes; }); break;
        case LibrarySort::LastPlayed: by([](const Game& game) { return game.LastPlayed; }); break;
        case LibrarySort::Purchased: by([](const Game& game) { return game.Purchased; }); break;
        case LibrarySort::Reviews: by([](const Game& game) { return game.ReviewPercent; }); break;
        case LibrarySort::Metacritic: by([](const Game& game) { return game.Metacritic; }); break;
        case LibrarySort::Released: by([](const Game& game) { return game.Released; }); break;
        case LibrarySort::Achievements:
            by([](const Game& game) { return game.AchievementsTotal == 0 ? 0.0 : 1.0 + static_cast<double>(game.AchievementsDone) / game.AchievementsTotal; });
            break;
        default: break;
        }
        return view;
    }
}
