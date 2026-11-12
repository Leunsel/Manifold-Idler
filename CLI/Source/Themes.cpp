#include "Themes.h"

#include "Core/Errors.h"
#include "Core/Text.h"
#include "Http.h"
#include "Paths.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <system_error>

namespace Idler::Cli
{
    namespace
    {
        constexpr std::size_t kMostThemes = 40;
        constexpr std::size_t kMostVariants = 12;
        constexpr std::size_t kMostOptions = 8;
        constexpr std::size_t kMostColors = 6;
        constexpr std::size_t kLongestId = 32;
        constexpr std::size_t kLongestName = 48;
        constexpr std::size_t kLongestNote = 160;
        constexpr std::size_t kLongestPart = 64;
        constexpr std::size_t kLongestPath = 200;
        constexpr std::size_t kDeepestPath = 4;
        constexpr std::size_t kMostFiles = 200;
        constexpr std::uintmax_t kLargestInfo = 64u * 1024u;
        constexpr std::uintmax_t kLargestFile = 8u * 1024u * 1024u;

        struct FileType
        {
            std::string_view Extension;
            std::string_view Type;
        };

        constexpr FileType kTypes[] = {
            { "css", "text/css; charset=utf-8" },
            { "woff2", "font/woff2" },
            { "woff", "font/woff" },
            { "ttf", "font/ttf" },
            { "otf", "font/otf" },
            { "png", "image/png" },
            { "jpg", "image/jpeg" },
            { "jpeg", "image/jpeg" },
            { "gif", "image/gif" },
            { "webp", "image/webp" },
            { "svg", "image/svg+xml" },
        };

        enum class Part
        {
            Theme,
            Variant,
            Option,
            Skip
        };

        std::string Lower(std::string_view text)
        {
            std::string out(text);
            for (char& character : out)
                character = character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
            return out;
        }

        bool IsHex(char character)
        {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') || (character >= 'A' && character <= 'F');
        }

        bool IsColor(std::string_view text)
        {
            if (text.size() != 4 && text.size() != 5 && text.size() != 7 && text.size() != 9)
                return false;
            if (text.front() != '#')
                return false;
            return std::all_of(text.begin() + 1, text.end(), IsHex);
        }

        bool IsCleanText(std::string_view text)
        {
            return std::none_of(text.begin(), text.end(), [](char character) { return static_cast<unsigned char>(character) < 0x20 || character == 0x7F; });
        }

        bool YesWord(std::string_view word, bool& value)
        {
            const std::string lower = Lower(word);
            if (lower == "on" || lower == "yes" || lower == "true" || lower == "1")
                value = true;
            else if (lower == "off" || lower == "no" || lower == "false" || lower == "0")
                value = false;
            else
                return false;
            return true;
        }

        std::vector<std::string_view> Words(std::string_view text)
        {
            std::vector<std::string_view> words;
            std::size_t at = 0;
            while (at < text.size())
            {
                const std::size_t first = text.find_first_not_of(" \t,", at);
                if (first == std::string_view::npos)
                    break;
                const std::size_t last = text.find_first_of(" \t,", first);
                words.push_back(text.substr(first, last == std::string_view::npos ? std::string_view::npos : last - first));
                at = last == std::string_view::npos ? text.size() : last;
            }
            return words;
        }

        std::string ThemeProblem(std::string_view detail)
        {
            return ErrorMessage(ErrorCode::ThemeNotRight, detail);
        }

        bool ReadWhole(const std::filesystem::path& file, std::uintmax_t largest, std::string& bytes)
        {
            std::error_code error;
            const std::uintmax_t size = std::filesystem::file_size(file, error);
            if (error || size > largest)
                return false;
            std::ifstream in(file, std::ios::binary);
            if (!in)
                return false;
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return bytes.size() == size;
        }

        bool Inside(const std::filesystem::path& folder, const std::filesystem::path& file)
        {
            std::error_code error;
            const std::filesystem::path base = std::filesystem::canonical(folder, error);
            if (error)
                return false;
            const std::filesystem::path real = std::filesystem::canonical(file, error);
            if (error)
                return false;
            auto inReal = real.begin();
            for (auto inBase = base.begin(); inBase != base.end(); ++inBase, ++inReal)
            {
                if (inReal == real.end() || *inReal != *inBase)
                    return false;
            }
            return inReal != real.end();
        }

        bool OwnTheme(const std::filesystem::path& folder, std::string_view id, std::filesystem::path& themeFolder)
        {
            if (folder.empty() || !IsThemeId(id))
                return false;
            themeFolder = folder / PathFromUtf8(id);
            std::error_code error;
            return std::filesystem::is_regular_file(themeFolder / "theme.css", error);
        }

        const EmbeddedFile* BuiltInFile(std::string_view path)
        {
            for (const EmbeddedFile& file : BuiltInThemeFiles())
            {
                if (file.Path == path)
                    return &file;
            }
            return nullptr;
        }

        std::vector<std::string> BuiltInIds()
        {
            std::vector<std::string> ids;
            for (const EmbeddedFile& file : BuiltInThemeFiles())
            {
                const std::size_t slash = file.Path.find('/');
                if (slash != std::string::npos && std::string_view(file.Path).substr(slash + 1) == "theme.css")
                    ids.push_back(file.Path.substr(0, slash));
            }
            return ids;
        }

        Theme Finished(Theme theme)
        {
            if (theme.Name.empty())
                theme.Name = theme.Id;
            for (ThemeVariant& variant : theme.Variants)
            {
                if (variant.Name.empty())
                    variant.Name = variant.Id;
            }
            for (ThemeOption& option : theme.Options)
            {
                if (option.Name.empty())
                    option.Name = option.Id;
            }
            if (theme.Swatch.empty() && !theme.Variants.empty())
                theme.Swatch = theme.Variants.front().Swatch;
            return theme;
        }

        std::string JsonList(const std::vector<std::string>& items)
        {
            std::string json = "[";
            for (std::size_t index = 0; index < items.size(); ++index)
                json += (index > 0 ? "," : "") + JsonString(items[index]);
            return json + "]";
        }

        std::string Padded(std::string text, std::size_t width)
        {
            if (text.size() < width)
                text.append(width - text.size(), ' ');
            return text;
        }

        std::string Renamed(std::string_view info, std::string_view name)
        {
            const std::string nameLine = "name=" + std::string(name) + "\n";
            std::string out;
            bool inTheme = true;
            bool done = false;
            std::size_t start = 0;
            while (start < info.size())
            {
                const std::size_t end = std::min(info.find('\n', start), info.size());
                const std::string_view line = info.substr(start, end - start);
                start = end + 1;
                const std::string_view bare = Trim(line);
                if (bare.size() >= 2 && bare.front() == '[' && bare.back() == ']')
                {
                    const bool theme = Lower(Trim(bare.substr(1, bare.size() - 2))) == "theme";
                    if (inTheme && !theme && !done)
                    {
                        out += nameLine;
                        done = true;
                    }
                    inTheme = theme;
                }
                else if (inTheme && !done && bare.find('=') != std::string_view::npos && Lower(Trim(bare.substr(0, bare.find('=')))) == "name")
                {
                    out += nameLine;
                    done = true;
                    continue;
                }
                out += line;
                out += '\n';
            }
            if (!done)
                out += inTheme ? nameLine : "\n[theme]\n" + nameLine;
            return out;
        }
    }

    bool IsThemeId(std::string_view text)
    {
        if (text.empty() || text.size() > kLongestId || text.front() == '-')
            return false;
        return std::all_of(text.begin(), text.end(), [](char character) { return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '-'; });
    }

    bool IsThemeFilePath(std::string_view path)
    {
        if (path.empty() || path.size() > kLongestPath)
            return false;
        std::size_t parts = 0;
        std::size_t start = 0;
        while (true)
        {
            const std::size_t slash = path.find('/', start);
            const std::string_view part = path.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
            if (part.empty() || part.size() > kLongestPart || part.front() == '.' || ++parts > kDeepestPath)
                return false;
            const bool plain = std::all_of(part.begin(), part.end(), [](char character)
                {
                    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
                        character == '.' || character == '_' || character == '-';
                });
            if (!plain)
                return false;
            if (slash == std::string_view::npos)
                return true;
            start = slash + 1;
        }
    }

    std::string ThemeFileType(std::string_view path)
    {
        const std::size_t dot = path.rfind('.');
        const std::size_t slash = path.rfind('/');
        if (dot == std::string_view::npos || (slash != std::string_view::npos && dot < slash))
            return {};
        const std::string extension = Lower(path.substr(dot + 1));
        for (const FileType& type : kTypes)
        {
            if (type.Extension == extension)
                return std::string(type.Type);
        }
        return {};
    }

    void ReadThemeInfo(std::string_view text, Theme& theme, std::vector<std::string>& problems)
    {
        if (text.starts_with("\xEF\xBB\xBF"))
            text.remove_prefix(3);
        Part part = Part::Theme;
        std::size_t number = 0;
        const auto problem = [&](const std::string& detail) { problems.push_back(ThemeProblem(theme.Id + ", theme.ini line " + std::to_string(number) + ": " + detail)); };

        std::size_t start = 0;
        while (start < text.size())
        {
            const std::size_t end = std::min(text.find('\n', start), text.size());
            const std::string_view line = Trim(text.substr(start, end - start));
            start = end + 1;
            ++number;
            if (line.empty() || line.front() == ';' || line.front() == '#')
                continue;

            if (line.front() == '[')
            {
                part = Part::Skip;
                if (line.back() != ']')
                {
                    problem("a section ends with ]");
                    continue;
                }
                const std::string_view inside = Trim(line.substr(1, line.size() - 2));
                const std::size_t space = inside.find_first_of(" \t");
                const std::string kind = Lower(inside.substr(0, space));
                const std::string id(space == std::string_view::npos ? std::string_view() : Trim(inside.substr(space + 1)));
                if (kind == "theme" && id.empty())
                {
                    part = Part::Theme;
                    continue;
                }
                if (kind != "variant" && kind != "option")
                {
                    problem("[" + std::string(inside) + "] is no section of a theme; there are [theme], [variant <name>] and [option <name>]");
                    continue;
                }
                if (!IsThemeId(id))
                {
                    problem("\"" + id + "\" is not a name for a " + kind + "; a to z, 0 to 9 and - fit, 32 at most");
                    continue;
                }
                if (kind == "variant")
                {
                    if (std::any_of(theme.Variants.begin(), theme.Variants.end(), [&](const ThemeVariant& variant) { return variant.Id == id; }))
                        problem("[variant " + id + "] is there twice");
                    else if (theme.Variants.size() >= kMostVariants)
                        problem("a theme has " + std::to_string(kMostVariants) + " variants at most; the rest are left out");
                    else
                    {
                        theme.Variants.push_back({ id, {}, {} });
                        part = Part::Variant;
                    }
                    continue;
                }
                if (std::any_of(theme.Options.begin(), theme.Options.end(), [&](const ThemeOption& option) { return option.Id == id; }))
                    problem("[option " + id + "] is there twice");
                else if (theme.Options.size() >= kMostOptions)
                    problem("a theme has " + std::to_string(kMostOptions) + " options at most; the rest are left out");
                else
                {
                    theme.Options.push_back({ id, {}, {}, false });
                    part = Part::Option;
                }
                continue;
            }

            const std::size_t equals = line.find('=');
            if (equals == std::string_view::npos)
            {
                problem("a line holds a key, = and a value, such as name=Arcade");
                continue;
            }
            if (part == Part::Skip)
                continue;
            const std::string key = Lower(Trim(line.substr(0, equals)));
            const std::string_view value = Trim(line.substr(equals + 1));

            if (key == "name" || (key == "note" && part != Part::Variant))
            {
                const std::size_t longest = key == "name" ? kLongestName : kLongestNote;
                if (!IsCleanText(value) || value.size() > longest)
                {
                    problem(key + " holds " + std::to_string(longest) + " bytes at most and no control characters");
                    continue;
                }
                std::string& target = part == Part::Theme ? (key == "name" ? theme.Name : theme.Note)
                    : part == Part::Variant ? theme.Variants.back().Name
                    : (key == "name" ? theme.Options.back().Name : theme.Options.back().Note);
                target = std::string(value);
                continue;
            }
            if (key == "swatch" && part != Part::Option)
            {
                std::vector<std::string> colors;
                bool fine = true;
                for (const std::string_view word : Words(value))
                {
                    if (!IsColor(word))
                    {
                        problem("\"" + std::string(word) + "\" is not a colour such as #22e6ff");
                        fine = false;
                        break;
                    }
                    colors.push_back(Lower(word));
                }
                if (!fine)
                    continue;
                if (colors.size() > kMostColors)
                {
                    problem("a swatch has " + std::to_string(kMostColors) + " colours at most");
                    colors.resize(kMostColors);
                }
                (part == Part::Theme ? theme.Swatch : theme.Variants.back().Swatch) = std::move(colors);
                continue;
            }
            if (key == "default" && part == Part::Option)
            {
                if (!YesWord(value, theme.Options.back().On))
                    problem("default is on or off");
                continue;
            }
            const char* where = part == Part::Theme ? "[theme]" : part == Part::Variant ? "a variant" : "an option";
            problem("\"" + key + "\" is no key of " + where);
        }
    }

    ThemeSet LoadThemes(const std::filesystem::path& folder)
    {
        ThemeSet set;
        set.Folder = folder;
        for (const std::string& id : BuiltInIds())
        {
            Theme theme;
            theme.Id = id;
            theme.BuiltIn = true;
            if (const EmbeddedFile* info = BuiltInFile(id + "/theme.ini"))
                ReadThemeInfo(info->Bytes, theme, set.Problems);
            set.Themes.push_back(Finished(std::move(theme)));
        }

        std::error_code error;
        if (folder.empty() || !std::filesystem::is_directory(folder, error))
            return set;
        std::vector<std::filesystem::path> folders;
        for (std::filesystem::directory_iterator entry(folder, error), end; !error && entry != end; entry.increment(error))
        {
            std::error_code kind;
            if (entry->is_directory(kind))
                folders.push_back(entry->path());
        }
        std::sort(folders.begin(), folders.end());

        std::size_t own = 0;
        for (const std::filesystem::path& path : folders)
        {
            const std::string name = PathToUtf8(path.filename());
            if (name.empty() || name.front() == '.')
                continue;
            if (!IsThemeId(name))
            {
                set.Problems.push_back(ThemeProblem("\"" + name + "\": a theme's folder name holds only a to z, 0 to 9 and -, so it is left out"));
                continue;
            }
            if (!std::filesystem::is_regular_file(path / "theme.css", error))
            {
                set.Problems.push_back(ThemeProblem(name + ": its folder has no theme.css, so it is left out"));
                continue;
            }
            if (++own > kMostThemes)
            {
                set.Problems.push_back(ThemeProblem(name + ": there are more than " + std::to_string(kMostThemes) + " themes of your own; the rest are left out"));
                break;
            }
            Theme theme;
            theme.Id = name;
            theme.FromFolder = true;
            const std::filesystem::path infoFile = path / "theme.ini";
            if (std::filesystem::exists(infoFile, error))
            {
                std::string info;
                if (ReadWhole(infoFile, kLargestInfo, info))
                    ReadThemeInfo(info, theme, set.Problems);
                else
                    set.Problems.push_back(ThemeProblem(name + ": theme.ini cannot be read or is larger than 64 KB"));
            }
            theme = Finished(std::move(theme));
            const auto same = std::find_if(set.Themes.begin(), set.Themes.end(), [&](const Theme& known) { return known.Id == name; });
            if (same != set.Themes.end())
            {
                theme.BuiltIn = same->BuiltIn;
                *same = std::move(theme);
            }
            else
            {
                set.Themes.push_back(std::move(theme));
            }
        }
        return set;
    }

    std::string ThemesJson(const ThemeSet& themes)
    {
        std::string json = "{\"default\":" + JsonString(kDefaultTheme) + ",\"folder\":" + JsonString(themes.Folder.empty() ? std::string() : PathToUtf8(themes.Folder)) +
            ",\"docs\":" + JsonString(kThemesReference) + ",\"themes\":[";
        for (std::size_t index = 0; index < themes.Themes.size(); ++index)
        {
            const Theme& theme = themes.Themes[index];
            const char* from = !theme.FromFolder ? "built-in" : theme.BuiltIn ? "replaced" : "folder";
            json += std::string(index > 0 ? "," : "") + "{\"id\":" + JsonString(theme.Id) + ",\"name\":" + JsonString(theme.Name) + ",\"note\":" + JsonString(theme.Note) +
                ",\"from\":" + JsonString(from) + ",\"swatch\":" + JsonList(theme.Swatch) + ",\"variants\":[";
            for (std::size_t at = 0; at < theme.Variants.size(); ++at)
            {
                const ThemeVariant& variant = theme.Variants[at];
                json += std::string(at > 0 ? "," : "") + "{\"id\":" + JsonString(variant.Id) + ",\"name\":" + JsonString(variant.Name) + ",\"swatch\":" + JsonList(variant.Swatch) + "}";
            }
            json += "],\"options\":[";
            for (std::size_t at = 0; at < theme.Options.size(); ++at)
            {
                const ThemeOption& option = theme.Options[at];
                json += std::string(at > 0 ? "," : "") + "{\"id\":" + JsonString(option.Id) + ",\"name\":" + JsonString(option.Name) + ",\"note\":" + JsonString(option.Note) +
                    ",\"on\":" + (option.On ? "true" : "false") + "}";
            }
            json += "]}";
        }
        json += "],\"problems\":" + JsonList(themes.Problems) + "}";

        std::string safe;
        safe.reserve(json.size());
        for (const char character : json)
        {
            if (character == '<')
                safe += "\\u003c";
            else if (character == '>')
                safe += "\\u003e";
            else if (character == '&')
                safe += "\\u0026";
            else
                safe.push_back(character);
        }
        return safe;
    }

    std::string ThemesText(const ThemeSet& themes)
    {
        std::error_code error;
        std::string text = "Themes of your own: " + (themes.Folder.empty() ? std::string("none") : PathToUtf8(themes.Folder));
        if (!themes.Folder.empty() && !std::filesystem::is_directory(themes.Folder, error))
            text += " (that folder is not there yet)";
        text += "\n\n";

        std::size_t idWidth = 0;
        std::size_t nameWidth = 0;
        for (const Theme& theme : themes.Themes)
        {
            idWidth = std::max(idWidth, theme.Id.size());
            nameWidth = std::max(nameWidth, theme.Name.size());
        }
        for (const Theme& theme : themes.Themes)
        {
            const char* from = !theme.FromFolder ? "built in" : theme.BuiltIn ? "your own, in place of the built-in one" : "your own";
            std::string line = "  " + Padded(theme.Id, idWidth + 2) + Padded(theme.Name, nameWidth + 2) + from;
            if (!theme.Variants.empty())
            {
                line += "\n" + std::string(idWidth + 4, ' ') + "variants: ";
                for (std::size_t at = 0; at < theme.Variants.size(); ++at)
                    line += (at > 0 ? ", " : "") + theme.Variants[at].Id;
            }
            if (!theme.Options.empty())
            {
                line += "\n" + std::string(idWidth + 4, ' ') + "options: ";
                for (std::size_t at = 0; at < theme.Options.size(); ++at)
                    line += (at > 0 ? ", " : "") + theme.Options[at].Id + (theme.Options[at].On ? " (on)" : " (off)");
            }
            text += line + "\n";
        }
        text += "\nA copy to change: idler themes copy <theme> <new name>\n";
        if (!themes.Problems.empty())
        {
            text += "\n";
            for (const std::string& problem : themes.Problems)
                text += problem + "\n";
        }
        return text;
    }

    bool FindThemeFile(const std::filesystem::path& folder, std::string_view id, std::string_view file, std::string& bytes, std::string& type)
    {
        if (!IsThemeId(id) || !IsThemeFilePath(file))
            return false;
        type = ThemeFileType(file);
        if (type.empty())
            return false;
        std::filesystem::path themeFolder;
        if (OwnTheme(folder, id, themeFolder))
        {
            const std::filesystem::path wanted = themeFolder / PathFromUtf8(file);
            std::error_code error;
            if (std::filesystem::is_regular_file(wanted, error) && Inside(themeFolder, wanted) && ReadWhole(wanted, kLargestFile, bytes))
                return true;
        }
        if (const EmbeddedFile* embedded = BuiltInFile(std::string(id) + "/" + std::string(file)))
        {
            bytes = embedded->Bytes;
            return true;
        }
        return false;
    }

    bool CopyTheme(const std::filesystem::path& folder, std::string_view from, std::string_view name, std::string& text)
    {
        if (!IsThemeId(name))
        {
            text = ErrorMessage(ErrorCode::ThemeNameUnusable, name);
            return false;
        }
        std::map<std::string, std::string> files;
        if (IsThemeId(from))
        {
            const std::string prefix = std::string(from) + "/";
            for (const EmbeddedFile& file : BuiltInThemeFiles())
            {
                if (file.Path.starts_with(prefix))
                    files[file.Path.substr(prefix.size())] = file.Bytes;
            }
        }
        std::filesystem::path themeFolder;
        if (OwnTheme(folder, from, themeFolder))
        {
            std::error_code error;
            for (std::filesystem::recursive_directory_iterator entry(themeFolder, error), end; !error && entry != end; entry.increment(error))
            {
                std::error_code kind;
                if (!entry->is_regular_file(kind))
                    continue;
                std::string relative = PathToUtf8(entry->path().lexically_relative(themeFolder));
                std::replace(relative.begin(), relative.end(), '\\', '/');
                if (relative != "theme.ini" && (!IsThemeFilePath(relative) || ThemeFileType(relative).empty()))
                    continue;
                if (files.size() >= kMostFiles)
                    break;
                std::string bytes;
                if (Inside(themeFolder, entry->path()) && ReadWhole(entry->path(), kLargestFile, bytes))
                    files[relative] = std::move(bytes);
            }
        }
        if (files.find("theme.css") == files.end())
        {
            text = ErrorMessage(ErrorCode::ThemeUnknown, from);
            return false;
        }

        const std::filesystem::path target = folder / PathFromUtf8(name);
        std::error_code error;
        if (std::filesystem::exists(target, error))
        {
            text = ErrorMessage(ErrorCode::ThemeFolderTaken, PathToUtf8(target));
            return false;
        }
        files["theme.ini"] = Renamed(files["theme.ini"], name);
        for (const auto& [relative, bytes] : files)
        {
            const std::filesystem::path file = target / PathFromUtf8(relative);
            error.clear();
            std::filesystem::create_directories(file.parent_path(), error);
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.close();
            if (error || !out)
            {
                text = ErrorMessage(ErrorCode::ThemeNotWritten, PathToUtf8(file));
                return false;
            }
        }
        text = "Copied " + std::string(from) + " to " + PathToUtf8(target) + ".\nThe web page lists it as " + std::string(name) +
            " after a reload; theme.css and theme.ini there change it.";
        return true;
    }
}
