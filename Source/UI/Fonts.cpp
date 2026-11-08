#include "UI/Fonts.h"

#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"
#include "Resource.h"

#include "imgui.h"

#include <windows.h>

#include <initializer_list>
#include <string>

namespace Idler::Fonts
{
    namespace
    {
        ImFont* g_bold = nullptr;

        std::string SystemFont(const wchar_t* name)
        {
            wchar_t folder[MAX_PATH] = L"";
            const UINT length = ::GetWindowsDirectoryW(folder, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
                return {};
            const std::wstring path = std::wstring(folder) + L"\\Fonts\\" + name;
            if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES)
                return {};
            return WideToUtf8(path);
        }
    }

    bool Build(const char** error)
    {
        const char* dummy = nullptr;
        if (!error)
            error = &dummy;
        *error = nullptr;

        HMODULE module = ::GetModuleHandleW(nullptr);
        HRSRC resource = ::FindResourceW(module, MAKEINTRESOURCEW(IDR_FONT_FA_SOLID), RT_RCDATA);
        HGLOBAL handle = resource ? ::LoadResource(module, resource) : nullptr;
        void* icons = handle ? ::LockResource(handle) : nullptr;
        const int iconsSize = resource ? static_cast<int>(::SizeofResource(module, resource)) : 0;
        if (!icons || iconsSize <= 0)
        {
            *error = "The embedded icon font is missing. Was App.rc compiled in?";
            return false;
        }

        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        ImFontConfig textConfig{};
        textConfig.PixelSnapH = true;
        std::string text = SystemFont(L"tahoma.ttf");
        if (text.empty())
            text = SystemFont(L"segoeui.ttf");
        ImFont* font = text.empty() ? atlas->AddFontDefault(&textConfig) : atlas->AddFontFromFileTTF(text.c_str(), kSize, &textConfig);

        ImFontConfig iconConfig{};
        iconConfig.MergeMode = true;
        iconConfig.PixelSnapH = true;
        iconConfig.FontDataOwnedByAtlas = false;
        static const ImWchar iconRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        if (!font || !atlas->AddFontFromMemoryTTF(icons, iconsSize, kSize, &iconConfig, iconRanges))
        {
            *error = "Could not build the font atlas.";
            return false;
        }

        if (const std::string symbols = SystemFont(L"seguisym.ttf"); !symbols.empty())
        {
            ImFontConfig symbolConfig{};
            symbolConfig.MergeMode = true;
            atlas->AddFontFromFileTTF(symbols.c_str(), kSize, &symbolConfig);
        }

        std::string boldFile = SystemFont(L"tahomabd.ttf");
        if (boldFile.empty())
            boldFile = SystemFont(L"segoeuib.ttf");
        if (!boldFile.empty())
        {
            ImFontConfig boldConfig{};
            boldConfig.PixelSnapH = true;
            g_bold = atlas->AddFontFromFileTTF(boldFile.c_str(), kSize, &boldConfig);
            if (g_bold)
                atlas->AddFontFromMemoryTTF(icons, iconsSize, kSize, &iconConfig, iconRanges);
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
        const auto add = [&](std::initializer_list<const wchar_t*> names)
            {
                for (const wchar_t* name : names)
                {
                    const std::string file = SystemFont(name);
                    if (file.empty())
                        continue;
                    ImFontConfig config{};
                    config.MergeMode = true;
                    config.DstFont = font;
                    atlas->AddFontFromFileTTF(file.c_str(), kSize, &config);
                }
            };
        if (needs & kFontNeedOtherScripts)
            add({ L"himalaya.ttf", L"Nirmala.ttc", L"LeelawUI.ttf", L"ebrima.ttf", L"mmrtext.ttf" });
        if (needs & kFontNeedAsian)
            add({ L"msyh.ttc", L"YuGothR.ttc", L"malgun.ttf" });
    }
}
