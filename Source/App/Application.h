#pragma once

#include "App/IdlerModel.h"
#include "App/Shell.h"
#include "Core/Settings.h"
#include "Core/SmoothWheel.h"

#include <filesystem>
#include <string>
#include <vector>

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

namespace Idler
{
    class Application final : public Shell
    {
    public:
        explicit Application(std::filesystem::path settingsPath);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        bool Initialise(HINSTANCE instance, int showCommand);
        int Run();

        void SetScaleOverride(float scale) { m_scaleOverride = scale; }
        void SetStartMinimised(bool minimised) { m_startMinimised = minimised; }
        void SetOpenInBackground(bool background) { m_openInBackground = background; }
        void SetCapture(std::filesystem::path file) { m_captureFile = std::move(file); }
        void SetSplash(bool splash) { m_splash = splash; }
        const wchar_t* LastError() const { return m_error; }

        IdlerModel& Model() override { return m_model; }
        HWND Window() const { return m_hwnd; }
        void* NativeWindow() const override { return m_hwnd; }

        float CornerRadius() const override;
        void AddDragArea(float left, float top, float right, float bottom) override;

        void Minimise() override { ::ShowWindow(m_hwnd, SW_MINIMIZE); }
        bool Maximised() const override { return ::IsZoomed(m_hwnd) != FALSE; }
        float Zoom() const override { return m_zoom; }
        void ToggleMaximised() override { ::ShowWindow(m_hwnd, Maximised() ? SW_RESTORE : SW_MAXIMIZE); }
        void Close() override;
        bool HasTray() const override { return true; }
        void Quit() { m_running = false; }

        static UINT ShowWindowMessage();
        static const wchar_t* WindowClass();

    private:
        bool CreateAppWindow(HINSTANCE instance, int showCommand);
        bool CreateDeviceAndSwapChain();
        void DestroyDeviceAndSwapChain();
        bool CreateRenderTarget();
        void DestroyRenderTarget();

        void ApplyWindowChrome();
        void RescaleForDpi(UINT dpi);
        void UpdateZoom(UINT width, UINT height);
        float WindowScale() const;
        SIZE MinimumSize() const;
        void PlaceInitialWindow();
        void RememberPlacement();
        void RenderFrame();
        void ApplyPendingResize();
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

        static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
        LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

        Settings m_settings;
        IdlerModel m_model;

        HWND m_hwnd = nullptr;
        HINSTANCE m_instance = nullptr;

        ID3D11Device* m_device = nullptr;
        ID3D11DeviceContext* m_context = nullptr;
        IDXGISwapChain1* m_swapChain = nullptr;
        ID3D11RenderTargetView* m_renderTarget = nullptr;

        UINT m_pendingWidth = 0;
        UINT m_pendingHeight = 0;
        SmoothWheel m_wheel;
        bool m_occluded = false;
        bool m_inSizeMove = false;
        bool m_inFrame = false;
        bool m_running = false;
        UINT m_dpi = 96;
        float m_scaleOverride = 0.0f;
        float m_zoom = 1.0f;
        bool m_roundedCorners = false;
        std::vector<RECT> m_dragAreas;
        ULONGLONG m_lastInputTick = 0;

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

        void* m_baseStyle = nullptr;

        wchar_t m_error[256] = L"";
    };
}
