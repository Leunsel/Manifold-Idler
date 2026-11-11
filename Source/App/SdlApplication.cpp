#include "App/SdlApplication.h"

#include "App/Gl.h"
#include "App/WindowCommon.h"
#include "Core/Clock.h"
#include "UI/Covers.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/ScreenMain.h"
#include "UI/Theme.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"

#include <SDL3/SDL.h>

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>

namespace Idler
{
    namespace
    {
        using namespace WindowCommon;

        constexpr const char* kWindowTitle = "Manifold Idler";
        constexpr const char* kGlslVersion = "#version 130";
        constexpr std::uint64_t kDoubleClickMilliseconds = 450;

        Settings LoadSettings(const std::filesystem::path& path)
        {
            Settings settings;
            settings.Load(path);
            return settings;
        }

        SDL_HitTestResult SDLCALL HitTestThunk(SDL_Window*, const SDL_Point* point, void* data)
        {
            return static_cast<SDL_HitTestResult>(static_cast<const SdlApplication*>(data)->HitTest(point->x, point->y));
        }

        void WritePng(void* context, void* data, int size)
        {
            static_cast<std::ofstream*>(context)->write(static_cast<const char*>(data), size);
        }

        bool IsInput(std::uint32_t type)
        {
            switch (type)
            {
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_WHEEL:
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_TEXT_INPUT:
                return true;
            default:
                return false;
            }
        }
    }

    SdlApplication::SdlApplication(std::filesystem::path settingsPath)
        : m_settings(LoadSettings(settingsPath)), m_model(m_settings, std::move(settingsPath))
    {
    }

    SdlApplication::~SdlApplication()
    {
        m_model.Cards().CloseWeb();
        if (m_imguiStarted)
        {
            UI::Covers::Shutdown();
            ImGui_ImplOpenGL3_Shutdown();
            ImGui_ImplSDL3_Shutdown();
        }
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
        if (m_context)
            SDL_GL_DestroyContext(m_context);
        if (m_window)
            SDL_DestroyWindow(m_window);
        if (m_sdlStarted)
            SDL_Quit();
        delete static_cast<ImGuiStyle*>(m_baseStyle);
    }

    float SdlApplication::WindowScale() const
    {
        if (m_scaleOverride > 0.0f)
            return m_scaleOverride;
        if (!m_window)
            return 1.0f;
        const float display = SDL_GetWindowDisplayScale(m_window);
        const float density = SDL_GetWindowPixelDensity(m_window);
        const float scale = display > 0.0f && density > 0.0f ? display / density : 1.0f;
        return std::clamp(scale, 0.5f, 4.0f);
    }

    void SdlApplication::ApplyScale()
    {
        const float scale = WindowScale();
        if (scale != m_appliedScale)
        {
            m_appliedScale = scale;
            SDL_SetWindowMinimumSize(m_window, static_cast<int>(std::lround(kClientWidth * scale)), static_cast<int>(std::lround(kClientHeight * scale)));
        }
        if (m_baseStyle)
            ScaleStyle(*static_cast<const ImGuiStyle*>(m_baseStyle), scale * m_zoom);
    }

    void SdlApplication::UpdateZoom()
    {
        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSize(m_window, &width, &height) || width <= 0 || height <= 0)
            return;
        const float zoom = ZoomFor(static_cast<float>(width), static_cast<float>(height), WindowScale());
        if (zoom == m_zoom)
            return;
        m_zoom = zoom;
        ApplyScale();
    }

    void SdlApplication::PlaceInitialWindow()
    {
        const float scale = WindowScale();
        const int smallestWidth = static_cast<int>(std::lround(kClientWidth * scale));
        const int smallestHeight = static_cast<int>(std::lround(kClientHeight * scale));
        int width = std::max(smallestWidth, static_cast<int>(std::lround(m_settings.Width * scale)));
        int height = std::max(smallestHeight, static_cast<int>(std::lround(m_settings.Height * scale)));

        SDL_DisplayID display = SDL_GetDisplayForWindow(m_window);
        bool placed = false;
        const SDL_Rect saved{ m_settings.X, m_settings.Y, width, height };
        if (m_settings.Placed && !m_wayland)
        {
            int count = 0;
            if (SDL_DisplayID* displays = SDL_GetDisplays(&count))
            {
                for (int index = 0; index < count && !placed; ++index)
                {
                    SDL_Rect bounds{};
                    if (SDL_GetDisplayBounds(displays[index], &bounds) && SDL_HasRectIntersection(&saved, &bounds))
                    {
                        placed = true;
                        display = displays[index];
                    }
                }
                SDL_free(displays);
            }
        }

        SDL_Rect work{};
        if (display && SDL_GetDisplayUsableBounds(display, &work) && work.w > 0 && work.h > 0)
        {
            width = std::max(smallestWidth, std::min(width, work.w));
            height = std::max(smallestHeight, std::min(height, work.h));
        }
        SDL_SetWindowSize(m_window, width, height);
        if (placed)
            SDL_SetWindowPosition(m_window, saved.x, saved.y);
        else if (!m_wayland && work.w > 0)
            SDL_SetWindowPosition(m_window, work.x + (work.w - width) / 2, work.y + std::max(0, (work.h - height) / 2));
        m_normalWidth = width;
        m_normalHeight = height;
    }

    void SdlApplication::NoteNormalSize()
    {
        const SDL_WindowFlags flags = SDL_GetWindowFlags(m_window);
        if (flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_HIDDEN))
            return;
        int width = 0;
        int height = 0;
        if (SDL_GetWindowSize(m_window, &width, &height) && width > 0 && height > 0)
        {
            m_normalWidth = width;
            m_normalHeight = height;
        }
    }

    void SdlApplication::RememberPlacement()
    {
        if (!m_window || m_startMinimised)
            return;

        NoteNormalSize();
        const float scale = WindowScale();
        if (m_normalWidth > 0 && m_normalHeight > 0)
        {
            m_settings.Width = static_cast<int>(std::lround(m_normalWidth / scale));
            m_settings.Height = static_cast<int>(std::lround(m_normalHeight / scale));
        }
        const SDL_WindowFlags flags = SDL_GetWindowFlags(m_window);
        m_settings.Maximised = (flags & SDL_WINDOW_MAXIMIZED) != 0;
        int x = 0;
        int y = 0;
        if (!m_wayland && !(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) && SDL_GetWindowPosition(m_window, &x, &y))
        {
            m_settings.X = x;
            m_settings.Y = y;
            m_settings.Placed = true;
        }
    }

    void SdlApplication::AddDragArea(float left, float top, float right, float bottom)
    {
        m_dragAreas.push_back(Area{ left, top, right, bottom });
    }

    void SdlApplication::Minimise()
    {
        SDL_MinimizeWindow(m_window);
    }

    bool SdlApplication::Maximised() const
    {
        return m_window && (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0;
    }

    void SdlApplication::ToggleMaximised()
    {
        if (Maximised())
            SDL_RestoreWindow(m_window);
        else
            SDL_MaximizeWindow(m_window);
    }

    void SdlApplication::Close()
    {
        if (HasTray() && m_settings.KeepInTray && m_model.Active())
            Minimise();
        else
            Quit();
    }

    int SdlApplication::HitTest(int x, int y) const
    {
        if (!Maximised())
        {
            int width = 0;
            int height = 0;
            SDL_GetWindowSize(m_window, &width, &height);
            const float scale = WindowScale();
            const int edge = static_cast<int>(std::lround(kResizeEdge * scale));
            const int corner = static_cast<int>(std::lround(kResizeCorner * scale));
            const bool nearLeft = x < corner;
            const bool nearRight = x >= width - corner;
            const bool nearTop = y < corner;
            const bool nearBottom = y >= height - corner;
            if (y < edge)
                return nearLeft ? SDL_HITTEST_RESIZE_TOPLEFT : nearRight ? SDL_HITTEST_RESIZE_TOPRIGHT : SDL_HITTEST_RESIZE_TOP;
            if (y >= height - edge)
                return nearLeft ? SDL_HITTEST_RESIZE_BOTTOMLEFT : nearRight ? SDL_HITTEST_RESIZE_BOTTOMRIGHT : SDL_HITTEST_RESIZE_BOTTOM;
            if (x < edge)
                return nearTop ? SDL_HITTEST_RESIZE_TOPLEFT : nearBottom ? SDL_HITTEST_RESIZE_BOTTOMLEFT : SDL_HITTEST_RESIZE_LEFT;
            if (x >= width - edge)
                return nearTop ? SDL_HITTEST_RESIZE_TOPRIGHT : nearBottom ? SDL_HITTEST_RESIZE_BOTTOMRIGHT : SDL_HITTEST_RESIZE_RIGHT;
        }

        for (const Area& area : m_dragAreas)
        {
            if (x >= std::lround(area.Left) && x < std::lround(area.Right) && y >= std::lround(area.Top) && y < std::lround(area.Bottom))
                return SDL_HITTEST_DRAGGABLE;
        }
        return SDL_HITTEST_NORMAL;
    }

    bool SdlApplication::Initialise()
    {
        SDL_SetHint(SDL_HINT_APP_ID, "manifold-idler");
        SDL_SetAppMetadata(kWindowTitle, nullptr, "manifold-idler");
        if (m_openInBackground)
            SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            m_error = std::string("Could not start SDL: ") + SDL_GetError();
            return false;
        }
        m_sdlStarted = true;
        const char* driver = SDL_GetCurrentVideoDriver();
        m_wayland = driver && std::strcmp(driver, "wayland") == 0;

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

        const SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_BORDERLESS;
        m_window = SDL_CreateWindow(kWindowTitle, kClientWidth, kClientHeight, flags);
        if (!m_window)
        {
            m_error = std::string("Could not create the window: ") + SDL_GetError();
            return false;
        }
        m_context = SDL_GL_CreateContext(m_window);
        if (!m_context)
        {
            m_error = std::string("Could not create the OpenGL context: ") + SDL_GetError();
            return false;
        }
        SDL_GL_MakeCurrent(m_window, m_context);
        SDL_GL_SetSwapInterval(1);
        if (!Gl::Load())
        {
            m_error = "OpenGL 3 is not available.";
            return false;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigDpiScaleFonts = false;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ManifoldTheme::Apply(ImGui::GetStyle());
        m_baseStyle = new ImGuiStyle(ImGui::GetStyle());

        const char* fontError = nullptr;
        if (!Fonts::Build(&fontError))
        {
            m_error = fontError ? fontError : "font error";
            return false;
        }
        if (!ImGui_ImplSDL3_InitForOpenGL(m_window, m_context) || !ImGui_ImplOpenGL3_Init(kGlslVersion))
        {
            m_error = "Could not initialise Dear ImGui.";
            return false;
        }
        m_imguiStarted = true;
#if !defined(_WIN32)
        ImGui::GetMainViewport()->PlatformHandleRaw = m_window;
#endif

        PlaceInitialWindow();
        UpdateZoom();
        ApplyScale();
        SDL_SetWindowHitTest(m_window, &HitTestThunk, this);

        UI::Covers::Initialise(nullptr);
        m_model.Cards().SetWindow(m_window);
        UI::IdleAnimation::SetEnabled(true);

        if (m_settings.Maximised && !m_startMinimised)
            SDL_MaximizeWindow(m_window);
        SDL_ShowWindow(m_window);
        if (m_startMinimised)
        {
            SDL_MinimizeWindow(m_window);
            m_startMinimised = false;
        }
        m_lastInputTick = SDL_GetTicks();
        m_running = true;
        return true;
    }

    void SdlApplication::HandleEvent(SDL_Event& event)
    {
        if (IsInput(event.type))
            m_lastInputTick = SDL_GetTicks();

        if (event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.y != 0.0f)
        {
            m_wheel.Add(event.wheel.y);
            if (event.wheel.x == 0.0f)
                return;
            event.wheel.y = 0.0f;
            event.wheel.integer_y = 0;
        }

        ImGui_ImplSDL3_ProcessEvent(&event);

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            Quit();
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            Close();
            break;
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            ApplyScale();
            UpdateZoom();
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            NoteNormalSize();
            UpdateZoom();
            break;
        case SDL_EVENT_WINDOW_HIT_TEST:
        {
            const std::uint64_t now = SDL_GetTicks();
            float x = 0.0f;
            float y = 0.0f;
            SDL_GetMouseState(&x, &y);
            if (now - m_lastHitTestTick < kDoubleClickMilliseconds && HitTest(static_cast<int>(x), static_cast<int>(y)) == SDL_HITTEST_DRAGGABLE)
            {
                ToggleMaximised();
                m_lastHitTestTick = 0;
            }
            else
                m_lastHitTestTick = now;
            break;
        }
        default:
            break;
        }
    }

    void SdlApplication::RenderFrame()
    {
        if (const unsigned needs = m_model.TakeFontNeeds(); needs != 0)
            Fonts::AddFallbacks(needs);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        if (const float wheel = m_wheel.Take(ImGui::GetIO().DeltaTime, UI::IdleAnimation::Enabled()); wheel != 0.0f)
            ImGui::GetIO().AddMouseWheelEvent(0.0f, wheel);
        ImGui::GetIO().ConfigInputTrickleEventQueue = ClicksOrKeysQueued();
        ImGui::NewFrame();

        m_model.Update();
        UI::Covers::SetSteamRoot(m_model.SteamStatus().Root);
        UI::Covers::SetPictureScale(WindowScale() * m_zoom);
        UI::Covers::NewFrame();
        UI::IdleAnimation::BeginFrame();
        m_dragAreas.clear();
        UI::DrawMain(*this);

        ImGui::Render();

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        const ImVec4 clear = ManifoldTheme::Page;
        Gl::Clear(width, height, clear.x, clear.y, clear.z);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        CaptureIfDue();
        SDL_GL_SwapWindow(m_window);
    }

    void SdlApplication::CaptureIfDue()
    {
        if (m_captureFile.empty())
            return;

        const double now = m_model.Now();
        if (m_captureStart < 0.0)
            m_captureStart = now;
        const bool waiting = !m_model.ScannedOnce() || UI::Covers::Loading();
        if (waiting && now - m_captureStart < kCaptureTimeoutSeconds)
        {
            m_captureFrames = 0;
            return;
        }
        if (++m_captureFrames < kCaptureSettleFrames)
            return;

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        std::vector<std::uint8_t> pixels;
        if (Gl::ReadPixels(width, height, pixels))
        {
            std::ofstream file(m_captureFile, std::ios::binary | std::ios::trunc);
            stbi_write_png_to_func(&WritePng, &file, width, height, 4, pixels.data(), width * 4);
        }
        Quit();
    }

    int SdlApplication::Run()
    {
        while (m_running)
        {
            const SDL_WindowFlags flags = SDL_GetWindowFlags(m_window);
            const bool hidden = (flags & (SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_OCCLUDED)) != 0;

            std::int32_t timeout = 0;
            if (hidden)
                timeout = static_cast<std::int32_t>(kHiddenUpdateMilliseconds);
            else if (m_captureFile.empty() && !UI::IdleAnimation::NeedsFullRate() && SDL_GetTicks() - m_lastInputTick > kBusyMilliseconds)
                timeout = static_cast<std::int32_t>(UI::IdleAnimation::NeedsFrames() ? kAnimationFrameMilliseconds : kIdleFrameMilliseconds);

            SDL_Event event;
            if (timeout > 0 && SDL_WaitEventTimeout(&event, timeout))
                HandleEvent(event);
            while (m_running && SDL_PollEvent(&event))
                HandleEvent(event);
            if (!m_running)
                break;

            if (hidden)
            {
                m_model.Update();
                continue;
            }
            RenderFrame();
        }

        RememberPlacement();
        m_model.Finish();
        return 0;
    }
}
