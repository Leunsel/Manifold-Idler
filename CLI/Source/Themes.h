#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::Cli
{
    inline constexpr std::string_view kDefaultTheme = "arcade";
    inline constexpr std::string_view kThemesReference = "https://github.com/Leunsel/Manifold-Idler/blob/main/Docs/WEB-THEMES.md";

    struct ThemeVariant
    {
        std::string Id;
        std::string Name;
        std::vector<std::string> Swatch;
    };

    struct ThemeOption
    {
        std::string Id;
        std::string Name;
        std::string Note;
        bool On = false;
    };

    struct Theme
    {
        std::string Id;
        std::string Name;
        std::string Note;
        std::vector<std::string> Swatch;
        std::vector<ThemeVariant> Variants;
        std::vector<ThemeOption> Options;
        bool BuiltIn = false;
        bool FromFolder = false;
    };

    struct ThemeSet
    {
        std::filesystem::path Folder;
        std::vector<Theme> Themes;
        std::vector<std::string> Problems;
    };

    struct EmbeddedFile
    {
        std::string Path;
        std::string Bytes;
    };

    const std::vector<EmbeddedFile>& BuiltInThemeFiles();

    bool IsThemeId(std::string_view text);
    bool IsThemeFilePath(std::string_view path);
    std::string ThemeFileType(std::string_view path);
    void ReadThemeInfo(std::string_view text, Theme& theme, std::vector<std::string>& problems);
    ThemeSet LoadThemes(const std::filesystem::path& folder);
    std::string ThemesJson(const ThemeSet& themes);
    std::string ThemesText(const ThemeSet& themes);
    bool FindThemeFile(const std::filesystem::path& folder, std::string_view id, std::string_view file, std::string& bytes, std::string& type);
    bool CopyTheme(const std::filesystem::path& folder, std::string_view from, std::string_view name, std::string& text);
}
