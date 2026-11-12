#include "UI/Fonts.h"

#include "Core/Errors.h"
#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"

#include "imgui.h"

#if !defined(_WIN32) && !defined(IDLER_BUNDLED_FONTS)
#define IDLER_BUNDLED_FONTS
#endif

#ifdef IDLER_BUNDLED_FONTS
#include "UI/FontData.h"
#endif

#ifdef _WIN32
#include "Resource.h"
#include <windows.h>
#else
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#endif

#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>

namespace Idler::Fonts
{
    namespace
    {
        ImFont* g_bold = nullptr;

        struct Source
        {
            std::string File;
            const void* Data = nullptr;
            int Size = 0;
            int Index = 0;

            bool Found() const { return !File.empty() || (Data && Size > 0); }
        };

        ImFont* Add(ImFontAtlas* atlas, const Source& source, ImFontConfig& config, const ImWchar* ranges = nullptr)
        {
            config.FontNo = source.Index;
            if (!source.File.empty())
                return atlas->AddFontFromFileTTF(source.File.c_str(), kSize, &config, ranges);
            config.FontDataOwnedByAtlas = false;
            return atlas->AddFontFromMemoryTTF(const_cast<void*>(source.Data), source.Size, kSize, &config, ranges);
        }

#ifdef _WIN32
        Source SystemFont(const wchar_t* name)
        {
            wchar_t folder[MAX_PATH] = L"";
            const UINT length = ::GetWindowsDirectoryW(folder, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
                return {};
            const std::wstring path = std::wstring(folder) + L"\\Fonts\\" + name;
            if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
                return {};
            return Source{ WideToUtf8(path) };
        }

        Source SymbolFont()
        {
            return SystemFont(L"seguisym.ttf");
        }

        std::vector<Source> FallbackFonts(unsigned needs)
        {
            std::vector<Source> found;
            const auto add = [&](std::initializer_list<const wchar_t*> names)
                {
                    for (const wchar_t* name : names)
                    {
                        if (Source source = SystemFont(name); source.Found())
                            found.push_back(std::move(source));
                    }
                };
            if (needs & kFontNeedOtherScripts)
                add({ L"himalaya.ttf", L"Nirmala.ttc", L"LeelawUI.ttf", L"ebrima.ttf", L"mmrtext.ttf" });
            if (needs & kFontNeedAsian)
                add({ L"msyh.ttc", L"YuGothR.ttc", L"malgun.ttf" });
            return found;
        }
#else
        Source Match(const std::string& pattern)
        {
            const std::string command = "fc-match -f '%{file}\\n%{index}' '" + pattern + "' 2>/dev/null";
            FILE* pipe = ::popen(command.c_str(), "r");
            if (!pipe)
                return {};
            std::string answer;
            char buffer[512];
            while (const std::size_t read = std::fread(buffer, 1, sizeof(buffer), pipe))
                answer.append(buffer, read);
            ::pclose(pipe);

            const std::size_t line = answer.find('\n');
            Source source{ answer.substr(0, line) };
            if (line != std::string::npos)
                source.Index = std::atoi(answer.c_str() + line + 1);
            std::string extension = std::filesystem::path(source.File).extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(c | 0x20); });
            std::error_code error;
            if (source.File.empty() || (extension != ".ttf" && extension != ".ttc" && extension != ".otf") || !std::filesystem::is_regular_file(source.File, error))
                return {};
            return source;
        }

        Source SymbolFont()
        {
            return Match("sans-serif:charset=2605 2665");
        }

        std::vector<Source> FallbackFonts(unsigned needs)
        {
            std::vector<Source> found;
            const auto add = [&](std::initializer_list<const char*> languages)
                {
                    for (const char* language : languages)
                    {
                        Source source = Match(std::string("sans-serif:lang=") + language);
                        const bool known = std::any_of(found.begin(), found.end(), [&](const Source& other) { return other.File == source.File && other.Index == source.Index; });
                        if (source.Found() && !known)
                            found.push_back(std::move(source));
                    }
                };
            if (needs & kFontNeedOtherScripts)
                add({ "bo", "hi", "th", "am", "my" });
            if (needs & kFontNeedAsian)
                add({ "zh-cn", "ja", "ko" });
            return found;
        }
#endif

#ifdef IDLER_BUNDLED_FONTS
        Source Embedded(FontData::Blob blob)
        {
            return Source{ {}, blob.Data, blob.Size };
        }

        Source IconFont()
        {
            return Embedded(FontData::Icons());
        }

        Source TextFont()
        {
            return Embedded(FontData::Text());
        }

        Source BoldFont()
        {
            return Embedded(FontData::Bold());
        }
#else
        Source IconFont()
        {
            HMODULE module = ::GetModuleHandleW(nullptr);
            HRSRC resource = ::FindResourceW(module, MAKEINTRESOURCEW(IDR_FONT_FA_SOLID), RT_RCDATA);
            HGLOBAL handle = resource ? ::LoadResource(module, resource) : nullptr;
            void* icons = handle ? ::LockResource(handle) : nullptr;
            const int iconsSize = resource ? static_cast<int>(::SizeofResource(module, resource)) : 0;
            if (!icons || iconsSize <= 0)
                return {};
            return Source{ {}, icons, iconsSize };
        }

        Source TextFont()
        {
            Source text = SystemFont(L"tahoma.ttf");
            if (!text.Found())
                text = SystemFont(L"segoeui.ttf");
            return text;
        }

        Source BoldFont()
        {
            Source bold = SystemFont(L"tahomabd.ttf");
            if (!bold.Found())
                bold = SystemFont(L"segoeuib.ttf");
            return bold;
        }
#endif
    }

    bool Build(const char** error)
    {
        const char* dummy = nullptr;
        if (!error)
            error = &dummy;
        *error = nullptr;
        static std::string message;

        const Source icons = IconFont();
        if (!icons.Found())
        {
            message = ErrorMessage(ErrorCode::IconFontMissing);
            *error = message.c_str();
            return false;
        }

        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        ImFontConfig textConfig{};
        textConfig.PixelSnapH = true;
        const Source text = TextFont();
        ImFont* font = text.Found() ? Add(atlas, text, textConfig) : atlas->AddFontDefault(&textConfig);

        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.FontDataOwnedByAtlas = false;
        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        if (!font || !Add(atlas, icons, iconConfig, iconRanges))
        {
            message = ErrorMessage(ErrorCode::FontAtlasFailed);
            *error = message.c_str();
            return false;
        }

        if (const Source symbols = SymbolFont(); symbols.Found())
        {
            ImFontConfig symbolConfig{};
            symbolConfig.MergeMode = true;
            Add(atlas, symbols, symbolConfig);
        }

        if (const Source bold = BoldFont(); bold.Found())
        {
            ImFontConfig boldConfig{};
            boldConfig.PixelSnapH = true;
            g_bold = Add(atlas, bold, boldConfig);
            if (g_bold)
                Add(atlas, icons, iconConfig, iconRanges);
        }

        ImGui::GetIO().FontDefault = font;
        return true;
    }

    ImFont* Bold()
    {
        return g_bold ? g_bold : ImGui::GetIO().FontDefault;
    }

    void AddFallbacks(unsigned needs)
    {
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        ImFont* font = ImGui::GetIO().FontDefault;
        for (const Source& source : FallbackFonts(needs))
        {
            ImFontConfig config{};
            config.MergeMode = true;
            config.DstFont = font;
            Add(atlas, source, config);
        }
    }
}
