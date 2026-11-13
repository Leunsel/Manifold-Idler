#pragma once

#include "App/IdlerModel.h"
#include "App/Shell.h"
#include "Core/Settings.h"
#include "Core/SmoothWheel.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_GLContextState;
union SDL_Event;

namespace Idler
{
    class SdlApplication final : public Shell
    {
    public:
        explicit SdlApplication(std::filesystem::path settingsPath);
        ~SdlApplication();

        SdlApplication(const SdlApplication&) = delete;
        SdlApplication& operator=(const SdlApplication&) = delete;

        bool Initialise();
        int Run();

        void SetScaleOverride(float scale) { m_scaleOverride = scale; }
        void SetStartMinimised(bool minimised) { m_startMinimised = minimised; }
        void SetOpenInBackground(bool background) { m_openInBackground = background; }
        void SetCapture(std::filesystem::path file) { m_captureFile = std::move(file); }
        void SetSplash(bool) {}
        const std::string& LastError() const { return m_error; }

        IdlerModel& Model() override { return m_model; }
        void* NativeWindow() const override { return m_window; }

        float CornerRadius() const override { return 0.0f; }
        void AddDragArea(float left, float top, float right, float bottom) override;

        void Minimise() override;
        bool Maximised() const override;
        void ToggleMaximised() override;
        float Zoom() const override { return m_zoom; }
        void Close() override;
        bool HasTray() const override { return false; }
        void Quit() { m_running = false; }
        void Restart() override
        {
            m_restart = true;
            Quit();
        }
        bool RestartRequested() const { return m_restart; }

        int HitTest(int x, int y) const;

    private:
        struct Area
        {
            float Left = 0.0f;
            float Top = 0.0f;
            float Right = 0.0f;
            float Bottom = 0.0f;
        };

        float WindowScale() const;
        void ApplyScale();
        void UpdateZoom();
        void PlaceInitialWindow();
        void NoteNormalSize();
        void RememberPlacement();
        void HandleEvent(SDL_Event& event);
        void RenderFrame();
        void CaptureIfDue();

        Settings m_settings;
        IdlerModel m_model;

        SDL_Window* m_window = nullptr;
        SDL_GLContextState* m_context = nullptr;
        bool m_sdlStarted = false;
        bool m_imguiStarted = false;
        bool m_wayland = false;

        SmoothWheel m_wheel;
        bool m_running = false;
        bool m_restart = false;
        float m_scaleOverride = 0.0f;
        float m_zoom = 1.0f;
        float m_appliedScale = 0.0f;
        std::vector<Area> m_dragAreas;
        std::uint64_t m_lastInputTick = 0;
        int m_normalWidth = 0;
        int m_normalHeight = 0;
        std::uint64_t m_lastHitTestTick = 0;

        bool m_startMinimised = false;
        bool m_openInBackground = false;

        std::filesystem::path m_captureFile;
        double m_captureStart = -1.0;
        int m_captureFrames = 0;

        void* m_baseStyle = nullptr;
        std::string m_error;
    };
}
