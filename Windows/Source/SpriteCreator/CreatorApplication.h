#pragma once

#include "App/WindowHost.h"
#include "SpriteCreator/CreatorScreen.h"

#include <filesystem>
#include <utility>

#include <windows.h>

namespace Idler
{
    class CreatorApplication final : private WindowHost::Client
    {
    public:
        CreatorApplication();

        CreatorApplication(const CreatorApplication&) = delete;
        CreatorApplication& operator=(const CreatorApplication&) = delete;

        bool Initialise(HINSTANCE instance, int showCommand);
        int Run();

        void SetScaleOverride(float scale) { m_host.SetScaleOverride(scale); }
        void SetCapture(std::filesystem::path file) { m_captureFile = std::move(file); }
        void SetCreatureFolder(std::filesystem::path folder) { m_screen.SetCreatureFolder(std::move(folder)); }
        SpriteCreator::Screen& Screen() { return m_screen; }
        const wchar_t* LastError() const { return m_host.LastError(); }

    private:
        void DrawFrame() override;
        void AfterDraw() override;
        void CloseRequested() override { m_running = false; }

        bool m_running = false;
        std::filesystem::path m_captureFile;
        int m_captureFrames = 0;
        SpriteCreator::Screen m_screen;
        WindowHost m_host;
    };
}
