#include "App/CommandLine.h"
#include "SpriteCreator/CreatorApplication.h"

#include <windows.h>

#include <cwchar>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{
    std::filesystem::path ProgramFolder()
    {
        std::wstring path(MAX_PATH, L'\0');
        while (true)
        {
            const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            if (length == 0)
                return {};
            if (length < path.size())
            {
                path.resize(length);
                return std::filesystem::path(path).parent_path();
            }
            path.resize(path.size() * 2);
        }
    }
}

int APIENTRY wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR commandLine, _In_ int showCommand)
{
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);

    Idler::CreatorApplication app;
    if (const std::wstring scale = Idler::CommandLine::Argument(commandLine, L"--scale="); !scale.empty())
    {
        std::wstring number = scale;
        for (wchar_t& c : number)
            c = c == L',' ? L'.' : c;
        const double value = std::wcstod(number.c_str(), nullptr);
        if (value >= 0.5 && value <= 4.0)
            app.SetScaleOverride(static_cast<float>(value));
    }
    if (const std::wstring capture = Idler::CommandLine::Argument(commandLine, L"--capture="); !capture.empty())
        app.SetCapture(capture);

    std::filesystem::path folder = Idler::CommandLine::Argument(commandLine, L"--creatures=");
    if (folder.empty())
        folder = Idler::SpriteCreator::FindCreatureFolder(ProgramFolder());
    if (folder.empty())
    {
        std::error_code ignored;
        folder = Idler::SpriteCreator::FindCreatureFolder(std::filesystem::current_path(ignored));
    }
    app.SetCreatureFolder(folder);

    if (!app.Initialise(instance, showCommand))
    {
        ::MessageBoxW(nullptr, app.LastError(), L"Sprite Creator could not be started", MB_ICONERROR | MB_OK);
        return 1;
    }
    return app.Run();
}
