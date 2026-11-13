#pragma once

#include "App/IdlerModel.h"
#include "App/Shell.h"
#include "App/WindowHost.h"
#include "Core/Settings.h"

#include <filesystem>
#include <string>

#include <windows.h>

namespace Idler
{
    class Application final : public Shell, private WindowHost::Client
    {
    public:
        explicit Application(std::filesystem::path settingsPath);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        bool Initialise(HINSTANCE instance, int showCommand);
        int Run();

        void SetScaleOverride(float scale) { m_host.SetScaleOverride(scale); }
        void SetStartMinimised(bool minimised) { m_startMinimised = minimised; }
        void SetOpenInBackground(bool background) { m_openInBackground = background; }
        void SetCapture(std::filesystem::path file) { m_captureFile = std::move(file); }
        void SetSplash(bool splash) { m_splash = splash; }
        const wchar_t* LastError() const { return m_host.LastError(); }

        IdlerModel& Model() override { return m_model; }
        HWND Window() const { return m_host.Window(); }
        void* NativeWindow() const override { return m_host.Window(); }

        float CornerRadius() const override { return m_host.CornerRadius(); }
        void AddDragArea(float left, float top, float right, float bottom) override { m_host.AddDragArea(left, top, right, bottom); }

        void Minimise() override { m_host.Minimise(); }
        bool Maximised() const override { return m_host.Maximised(); }
        float Zoom() const override { return m_host.Zoom(); }
        void ToggleMaximised() override { m_host.ToggleMaximised(); }
        void Close() override;
        bool HasTray() const override { return true; }
        void Quit() { m_running = false; }
        void Restart() override
        {
            m_restart = true;
            Quit();
        }
        bool RestartRequested() const { return m_restart; }

        static UINT ShowWindowMessage();
        static const wchar_t* WindowClass();

    private:
        void BeforeFrame() override;
        void DrawFrame() override;
        void AfterDraw() override;
        bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result) override;
        void CloseRequested() override { Close(); }

        bool CreateAppWindow(HINSTANCE instance, int showCommand);
        void RememberPlacement();
        void CaptureIfDue();
        void ShowSplash();
        bool ReadyToReveal() const;
        void Reveal();

        void AddTrayIcon();
        void RemoveTrayIcon();
        void UpdateTray();
        void ShowTrayMenu();
        void HideToTray();
        void RestoreFromTray();

        Settings m_settings;
        IdlerModel m_model;

        HINSTANCE m_instance = nullptr;
        bool m_running = false;
        bool m_restart = false;

        bool m_startMinimised = false;
        bool m_openInBackground = false;
        bool m_splash = false;
        bool m_revealPending = false;
        int m_revealCommand = SW_SHOWNORMAL;
        ULONGLONG m_splashSince = 0;
        bool m_trayIcon = false;
        std::wstring m_trayTip;
        UINT m_taskbarCreated = 0;

        std::filesystem::path m_captureFile;
        double m_captureStart = -1.0;
        int m_captureFrames = 0;

        WindowHost m_host;
    };
}
