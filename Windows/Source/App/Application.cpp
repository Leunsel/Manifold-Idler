#include "App/Application.h"

#include "App/Desktop.h"
#include "App/Splash.h"
#include "App/WindowCommon.h"
#include "Core/Errors.h"
#include "Core/Text.h"
#include "Resource.h"
#include "UI/Covers.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/ScreenMain.h"
#include "UI/Theme.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <dwmapi.h>
#include <dxgi1_3.h>
#include <shellapi.h>
#include <windowsx.h>

#include <stdio.h>
#include <wchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Idler
{
    namespace
    {
        constexpr const wchar_t* kWindowClass = kCardFarmEnabled ? L"ManifoldIdlerAltWindow" : L"ManifoldIdlerWindow";
        constexpr const wchar_t* kWindowTitle = kCardFarmEnabled ? L"Manifold Idler (test)" : L"Manifold Idler";

        using namespace WindowCommon;

        void PutError(wchar_t* target, std::size_t size, ErrorCode code, std::string_view detail = {})
        {
            ::wcsncpy_s(target, size, Utf8ToWide(ErrorMessage(code, detail)).c_str(), _TRUNCATE);
        }

        std::string HexText(HRESULT result)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "0x%08X", static_cast<unsigned>(result));
            return text;
        }

        constexpr DWORD kWindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN;

        constexpr DWORD kUseImmersiveDarkMode = 20;
        constexpr DWORD kWindowCornerPreference = 33;
        constexpr DWORD kBorderColor = 34;
        constexpr DWORD kCornerRound = 2;
        constexpr COLORREF kColorNone = 0xFFFFFFFE;
        constexpr float kCornerRadius = 8.0f;

        constexpr UINT kTrayMessage = WM_APP + 1;
        constexpr UINT kTrayIconId = 1;
        constexpr UINT kMenuOpen = 1;
        constexpr UINT kMenuToggle = 2;
        constexpr UINT kMenuQuit = 3;

        constexpr UINT_PTR kSizeMoveTimer = 1;

        void FollowSystemAnimations()
        {
            BOOL on = TRUE;
            if (!::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0))
                on = TRUE;
            UI::IdleAnimation::SetEnabled(on != FALSE);
        }

        Settings LoadSettings(const std::filesystem::path& path)
        {
            Settings settings;
            settings.Load(path);
            return settings;
        }
    }

    UINT Application::ShowWindowMessage()
    {
        static const UINT message = ::RegisterWindowMessageW(L"ManifoldIdlerShowWindow");
        return message;
    }

    const wchar_t* Application::WindowClass()
    {
        return kWindowClass;
    }

    Application::Application(std::filesystem::path settingsPath)
        : m_settings(LoadSettings(settingsPath)), m_model(m_settings, std::move(settingsPath))
    {
    }

    Application::~Application()
    {
        Splash::Close();
        m_model.Cards().CloseWeb();
        RemoveTrayIcon();
        UI::Covers::Shutdown();
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        DestroyDeviceAndSwapChain();
        if (m_hwnd)
            ::DestroyWindow(m_hwnd);
        ::UnregisterClassW(kWindowClass, m_instance);
        delete static_cast<ImGuiStyle*>(m_baseStyle);
    }

    bool Application::CreateAppWindow(HINSTANCE instance, int showCommand)
    {
        m_instance = instance;
        m_taskbarCreated = ::RegisterWindowMessageW(L"TaskbarCreated");

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &Application::WndProcThunk;
        wc.hInstance = instance;
        wc.hIcon = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), 0));
        wc.hIconSm = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kWindowClass;
        if (!::RegisterClassExW(&wc))
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::WindowClassFailed);
            return false;
        }

        m_hwnd = ::CreateWindowExW(0, kWindowClass, kWindowTitle, kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            kClientWidth, kClientHeight, nullptr, nullptr, instance, this);
        if (!m_hwnd)
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::WindowFailed);
            return false;
        }

        m_dpi = ::GetDpiForWindow(m_hwnd);
        PlaceInitialWindow();
        ApplyWindowChrome();
        const bool minimised = showCommand == SW_SHOWMINIMIZED || showCommand == SW_SHOWMINNOACTIVE || showCommand == SW_MINIMIZE;
        if (m_settings.Maximised && !minimised)
            showCommand = SW_SHOWMAXIMIZED;
        else if (m_openInBackground)
            showCommand = SW_SHOWNOACTIVATE;
        if (m_splash && !m_startMinimised)
        {
            m_revealPending = true;
            m_revealCommand = showCommand;
        }
        else if (!m_startMinimised)
        {
            ::ShowWindow(m_hwnd, showCommand);
            ::UpdateWindow(m_hwnd);
        }
        return true;
    }

    void Application::ShowSplash()
    {
        const RECT saved{ m_settings.X, m_settings.Y, m_settings.X + m_settings.Width, m_settings.Y + m_settings.Height };
        HMONITOR monitor = m_settings.Placed ? ::MonitorFromRect(&saved, MONITOR_DEFAULTTONULL) : nullptr;
        RECT around = saved;
        if (!monitor)
        {
            MONITORINFO info = { sizeof(info), {}, {}, 0 };
            if (::GetMonitorInfoW(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY), &info))
                around = info.rcWork;
        }

        std::optional<CreatureId> creature = m_model.Buddy();
        const CreatureSet& found = m_model.FoundCreatures();
        if (!creature && found.any())
        {
            std::size_t pick = static_cast<std::size_t>(::GetTickCount64() % found.count());
            for (std::size_t index = 0; index < found.size(); ++index)
            {
                if (found.test(index) && pick-- == 0)
                {
                    creature = static_cast<CreatureId>(index);
                    break;
                }
            }
        }
        const CreatureId shown = creature.value_or(CreatureId::Slime);
        BOOL animate = TRUE;
        if (!::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animate, 0))
            animate = TRUE;
        Splash::Show(m_instance, around, shown, m_model.EvolvedCreatures().test(static_cast<std::size_t>(shown)), animate != FALSE, !m_openInBackground);
        m_splashSince = ::GetTickCount64();
    }

    bool Application::ReadyToReveal() const
    {
        constexpr ULONGLONG kShortestSplashMilliseconds = 2000;
        constexpr ULONGLONG kLongestSplashMilliseconds = 6000;
        const ULONGLONG shown = ::GetTickCount64() - m_splashSince;
        if (shown < kShortestSplashMilliseconds)
            return false;
        return m_model.ScannedOnce() || !m_model.SteamStatus().LoggedOn() || shown > kLongestSplashMilliseconds;
    }

    void Application::Reveal()
    {
        m_revealPending = false;
        ApplyPendingResize();
        RenderFrame();
        if (!::IsWindowVisible(m_hwnd))
            ::ShowWindow(m_hwnd, m_revealCommand);
        ApplyPendingResize();
        RenderFrame();
        Splash::Close();
        m_lastInputTick = ::GetTickCount64();
    }

    SIZE Application::MinimumSize() const
    {
        const float scale = WindowScale();
        return { std::lround(kClientWidth * scale), std::lround(kClientHeight * scale) };
    }

    void Application::PlaceInitialWindow()
    {
        const float scale = WindowScale();
        const SIZE smallest = MinimumSize();
        SIZE size{ (std::max)(smallest.cx, std::lround(m_settings.Width * scale)), (std::max)(smallest.cy, std::lround(m_settings.Height * scale)) };
        int x = 0;
        int y = 0;
        bool placed = false;

        const RECT saved{ m_settings.X, m_settings.Y, m_settings.X + size.cx, m_settings.Y + size.cy };
        if (m_settings.Placed && ::MonitorFromRect(&saved, MONITOR_DEFAULTTONULL))
        {
            x = saved.left;
            y = saved.top;
            placed = true;
        }

        MONITORINFO info = { sizeof(info), {}, {}, 0 };
        const HMONITOR monitor = placed ? ::MonitorFromRect(&saved, MONITOR_DEFAULTTONEAREST) : ::MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor && ::GetMonitorInfoW(monitor, &info))
        {
            const LONG workWidth = info.rcWork.right - info.rcWork.left;
            const LONG workHeight = info.rcWork.bottom - info.rcWork.top;
            size.cx = (std::max)(smallest.cx, (std::min)(size.cx, workWidth));
            size.cy = (std::max)(smallest.cy, (std::min)(size.cy, workHeight));
            if (!placed)
            {
                x = info.rcWork.left + (workWidth - size.cx) / 2;
                y = info.rcWork.top + (std::max)(0L, (workHeight - size.cy) / 2);
            }
        }
        ::SetWindowPos(m_hwnd, nullptr, x, y, size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    void Application::RememberPlacement()
    {
        if (!m_hwnd || m_startMinimised)
            return;

        WINDOWPLACEMENT placement{ sizeof(placement) };
        if (!::GetWindowPlacement(m_hwnd, &placement))
            return;

        RECT rect = placement.rcNormalPosition;
        if (!::IsIconic(m_hwnd) && !::IsZoomed(m_hwnd) && ::IsWindowVisible(m_hwnd))
        {
            if (!::GetWindowRect(m_hwnd, &rect))
                return;
        }
        else
        {
            MONITORINFO info = { sizeof(info), {}, {}, 0 };
            if (::GetMonitorInfoW(::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &info))
                ::OffsetRect(&rect, info.rcWork.left - info.rcMonitor.left, info.rcWork.top - info.rcMonitor.top);
        }

        const float scale = WindowScale();
        m_settings.X = rect.left;
        m_settings.Y = rect.top;
        m_settings.Width = std::lround((rect.right - rect.left) / scale);
        m_settings.Height = std::lround((rect.bottom - rect.top) / scale);
        m_settings.Maximised = ::IsZoomed(m_hwnd) || (::IsIconic(m_hwnd) && (placement.flags & WPF_RESTORETOMAXIMIZED));
        m_settings.Placed = true;
    }

    void Application::ApplyWindowChrome()
    {
        const BOOL dark = TRUE;
        ::DwmSetWindowAttribute(m_hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));

        const DWORD corner = kCornerRound;
        m_roundedCorners = SUCCEEDED(::DwmSetWindowAttribute(m_hwnd, kWindowCornerPreference, &corner, sizeof(corner)));
        const COLORREF border = kColorNone;
        ::DwmSetWindowAttribute(m_hwnd, kBorderColor, &border, sizeof(border));
    }

    float Application::CornerRadius() const
    {
        return m_roundedCorners && !Maximised() ? kCornerRadius * WindowScale() : 0.0f;
    }

    void Application::AddDragArea(float left, float top, float right, float bottom)
    {
        m_dragAreas.push_back(RECT{ std::lround(left), std::lround(top), std::lround(right), std::lround(bottom) });
    }

    bool Application::CreateDeviceAndSwapChain()
    {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        D3D_FEATURE_LEVEL obtained = {};

        HRESULT hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels),
            D3D11_SDK_VERSION, &m_device, &obtained, &m_context);
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING && (flags & D3D11_CREATE_DEVICE_DEBUG))
        {
            flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
            hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, &m_device, &obtained, &m_context);
        }
        if (hr == DXGI_ERROR_UNSUPPORTED)
        {
            hr = ::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, ARRAYSIZE(levels),
                D3D11_SDK_VERSION, &m_device, &obtained, &m_context);
        }
        if (FAILED(hr))
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::DeviceFailed, HexText(hr));
            return false;
        }

        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* adapter = nullptr;
        IDXGIFactory2* factory = nullptr;
        auto release = [&]
            {
                if (factory) factory->Release();
                if (adapter) adapter->Release();
                if (dxgiDevice) dxgiDevice->Release();
            };

        hr = m_device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
        if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        if (FAILED(hr))
        {
            release();
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::FactoryFailed, HexText(hr));
            return false;
        }

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_NONE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

        hr = factory->CreateSwapChainForHwnd(m_device, m_hwnd, &desc, nullptr, nullptr, &m_swapChain);
        if (FAILED(hr))
        {
            release();
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::SwapChainFailed, HexText(hr));
            return false;
        }

        factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
        release();
        return CreateRenderTarget();
    }

    bool Application::CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr) || !backBuffer)
            return false;
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
        backBuffer->Release();
        return SUCCEEDED(hr);
    }

    void Application::DestroyRenderTarget()
    {
        if (m_renderTarget)
        {
            m_renderTarget->Release();
            m_renderTarget = nullptr;
        }
    }

    void Application::DestroyDeviceAndSwapChain()
    {
        DestroyRenderTarget();
        if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
        if (m_context) { m_context->Release(); m_context = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
    }

    float Application::WindowScale() const
    {
        return (m_scaleOverride > 0.0f ? m_scaleOverride : static_cast<float>(m_dpi) / 96.0f);
    }

    void Application::UpdateZoom(UINT width, UINT height)
    {
        const float zoom = ZoomFor(static_cast<float>(width), static_cast<float>(height), WindowScale());
        if (zoom == m_zoom)
            return;
        m_zoom = zoom;
        RescaleForDpi(m_dpi);
    }

    void Application::RescaleForDpi(UINT dpi)
    {
        m_dpi = dpi;
        ScaleStyle(*static_cast<const ImGuiStyle*>(m_baseStyle), WindowScale() * m_zoom);
    }

    void Application::ApplyPendingResize()
    {
        if (m_pendingWidth == 0 || m_pendingHeight == 0 || !m_swapChain)
            return;
        DestroyRenderTarget();
        m_swapChain->ResizeBuffers(0, m_pendingWidth, m_pendingHeight, DXGI_FORMAT_UNKNOWN, 0);
        UpdateZoom(m_pendingWidth, m_pendingHeight);
        m_pendingWidth = m_pendingHeight = 0;
        CreateRenderTarget();
    }

    void Application::RenderFrame()
    {
        if (m_inFrame)
            return;
        m_inFrame = true;
        struct FrameDone
        {
            bool& Flag;
            ~FrameDone() { Flag = false; }
        } frameDone{ m_inFrame };

        if (const unsigned needs = m_model.TakeFontNeeds(); needs != 0)
            Fonts::AddFallbacks(needs);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (const float wheel = m_wheel.Take(ImGui::GetIO().DeltaTime, UI::IdleAnimation::Enabled()); wheel != 0.0f)
            ImGui::GetIO().AddMouseWheelEvent(0.0f, wheel);
        ImGui::GetIO().ConfigInputTrickleEventQueue = ClicksOrKeysQueued();
        ImGui::NewFrame();

        m_model.Update();
        UpdateTray();
        UI::Covers::SetSteamRoot(m_model.SteamStatus().Root);
        UI::Covers::SetPictureScale(WindowScale() * m_zoom);
        UI::Covers::NewFrame();
        UI::IdleAnimation::BeginFrame();
        m_dragAreas.clear();
        UI::DrawMain(*this);

        ImGui::Render();

        const ImVec4 clear = ManifoldTheme::Page;
        const float rgba[4] = { clear.x, clear.y, clear.z, 1.0f };
        m_context->OMSetRenderTargets(1, &m_renderTarget, nullptr);
        m_context->ClearRenderTargetView(m_renderTarget, rgba);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        CaptureIfDue();

        const HRESULT hr = m_swapChain->Present(1, 0);
        m_occluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    void Application::CaptureIfDue()
    {
        if (m_captureFile.empty())
            return;

        const double now = m_model.Now();
        if (m_captureStart < 0.0)
            m_captureStart = now;
        const bool waiting = !m_model.ScannedOnce() || UI::Covers::Loading() || m_model.Cards().State() == LoginState::Checking;
        if (waiting && now - m_captureStart < kCaptureTimeoutSeconds)
        {
            m_captureFrames = 0;
            return;
        }
        if (++m_captureFrames < kCaptureSettleFrames)
            return;

        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer)
        {
            Desktop::SavePng(m_device, m_context, backBuffer, m_captureFile);
            backBuffer->Release();
        }
        Quit();
    }

    bool Application::Initialise(HINSTANCE instance, int showCommand)
    {
        m_instance = instance;
        if (m_splash && !m_startMinimised)
            ShowSplash();
        if (!CreateAppWindow(instance, showCommand))
            return false;
        if (!CreateDeviceAndSwapChain())
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigDpiScaleFonts = false;
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;

        ManifoldTheme::Apply(ImGui::GetStyle());
        m_baseStyle = new ImGuiStyle(ImGui::GetStyle());
        RescaleForDpi(m_dpi);

        const char* fontError = nullptr;
        if (!Fonts::Build(&fontError))
        {
            ::wcsncpy_s(m_error, Utf8ToWide(fontError ? fontError : ErrorMessage(ErrorCode::FontAtlasFailed)).c_str(), _TRUNCATE);
            return false;
        }

        if (!ImGui_ImplWin32_Init(m_hwnd) || !ImGui_ImplDX11_Init(m_device, m_context))
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::ImGuiFailed);
            return false;
        }
        UI::Covers::Initialise(m_device);
        m_model.Cards().SetWindow(m_hwnd);
        FollowSystemAnimations();

        if (m_startMinimised)
            HideToTray();
        m_lastInputTick = ::GetTickCount64();
        m_running = true;
        return true;
    }

    int Application::Run()
    {
        MSG msg = {};
        while (m_running)
        {
            while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
                if (msg.message == WM_QUIT)
                    m_running = false;
            }
            if (!m_running)
                break;

            if (m_revealPending)
            {
                m_model.Update();
                if (m_model.Scanning())
                    Splash::SetStage(Splash::Stage::ReadingLibrary);
                if (::IsWindowVisible(m_hwnd) || ReadyToReveal())
                    Reveal();
                else
                    ::MsgWaitForMultipleObjectsEx(0, nullptr, 15, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }

            if (!::IsWindowVisible(m_hwnd) || ::IsIconic(m_hwnd) ||
                (m_occluded && m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED))
            {
                m_model.Update();
                UpdateTray();
                ::MsgWaitForMultipleObjectsEx(0, nullptr, kHiddenUpdateMilliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }
            m_occluded = false;

            if (m_captureFile.empty() && !UI::IdleAnimation::NeedsFullRate() && ::GetTickCount64() - m_lastInputTick > kBusyMilliseconds)
                ::MsgWaitForMultipleObjectsEx(0, nullptr, UI::IdleAnimation::NeedsFrames() ? kAnimationFrameMilliseconds : kIdleFrameMilliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

            ApplyPendingResize();
            RenderFrame();
        }

        RememberPlacement();
        m_model.Finish();
        return 0;
    }

    void Application::AddTrayIcon()
    {
        if (m_trayIcon || !m_hwnd)
            return;
        NOTIFYICONDATAW data{ sizeof(data) };
        data.hWnd = m_hwnd;
        data.uID = kTrayIconId;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = kTrayMessage;
        data.hIcon = static_cast<HICON>(::LoadImageW(m_instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));
        ::wcsncpy_s(data.szTip, m_trayTip.empty() ? kWindowTitle : m_trayTip.c_str(), _TRUNCATE);
        m_trayIcon = ::Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (data.hIcon)
            ::DestroyIcon(data.hIcon);
    }

    void Application::RemoveTrayIcon()
    {
        if (!m_trayIcon)
            return;
        NOTIFYICONDATAW data{ sizeof(data) };
        data.hWnd = m_hwnd;
        data.uID = kTrayIconId;
        ::Shell_NotifyIconW(NIM_DELETE, &data);
        m_trayIcon = false;
    }

    void Application::UpdateTray()
    {
        const bool finished = m_model.TakeQueueFinished();
        const bool cardDropped = m_model.TakeCardDropped();
        const std::string creatureNews = m_model.TakeCreatureNews();

        if (m_model.TakeWantsWindow() && !::IsWindowVisible(m_hwnd))
            RestoreFromTray();

        if (::IsWindowVisible(m_hwnd))
        {
            RemoveTrayIcon();
            return;
        }
        AddTrayIcon();
        if (!m_trayIcon)
            return;

        NOTIFYICONDATAW data{ sizeof(data) };
        data.hWnd = m_hwnd;
        data.uID = kTrayIconId;

        const int running = m_model.RunningCount();
        std::wstring tip = kWindowTitle;
        if (m_model.Active())
        {
            tip += L": " + std::to_wstring(running) + (running == 1 ? L" game idling" : L" games idling");
            const QueueForecast forecast = m_model.Forecast();
            if (!forecast.Endless && forecast.Seconds > 0.0)
                tip += L", " + Utf8ToWide(DurationText(static_cast<std::uint64_t>(forecast.Seconds))) + L" left";
        }
        if (tip != m_trayTip)
        {
            m_trayTip = tip;
            data.uFlags |= NIF_TIP;
            ::wcsncpy_s(data.szTip, tip.c_str(), _TRUNCATE);
        }
        if (finished || cardDropped || !creatureNews.empty())
        {
            data.uFlags |= NIF_INFO;
            ::wcscpy_s(data.szInfoTitle, kWindowTitle);
            const std::wstring text = finished ? L"Every game in the queue reached its goal." : cardDropped ? L"A trading card dropped." : Utf8ToWide(creatureNews);
            ::wcsncpy_s(data.szInfo, text.c_str(), _TRUNCATE);
            data.dwInfoFlags = NIIF_INFO;
        }
        if (data.uFlags != 0)
            ::Shell_NotifyIconW(NIM_MODIFY, &data);
    }

    void Application::ShowTrayMenu()
    {
        HMENU menu = ::CreatePopupMenu();
        if (!menu)
            return;
        ::AppendMenuW(menu, MF_STRING, kMenuOpen, L"Open");
        ::SetMenuDefaultItem(menu, kMenuOpen, FALSE);
        ::AppendMenuW(menu, MF_STRING, kMenuToggle, m_model.Active() ? L"Stop idling" : L"Start idling");
        ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        ::AppendMenuW(menu, MF_STRING, kMenuQuit, L"Quit");

        POINT point{};
        ::GetCursorPos(&point);
        ::SetForegroundWindow(m_hwnd);
        const UINT choice = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, m_hwnd, nullptr);
        ::DestroyMenu(menu);

        if (choice == kMenuOpen)
            RestoreFromTray();
        else if (choice == kMenuToggle)
            m_model.SetActive(!m_model.Active());
        else if (choice == kMenuQuit)
            Quit();
    }

    void Application::HideToTray()
    {
        RememberPlacement();
        AddTrayIcon();
        ::ShowWindow(m_hwnd, SW_HIDE);
    }

    void Application::RestoreFromTray()
    {
        const bool maximise = m_startMinimised && m_settings.Maximised;
        m_startMinimised = false;
        ::ShowWindow(m_hwnd, maximise ? SW_SHOWMAXIMIZED : ::IsIconic(m_hwnd) ? SW_RESTORE : SW_SHOW);
        ::SetForegroundWindow(m_hwnd);
        m_lastInputTick = ::GetTickCount64();
        RemoveTrayIcon();
    }

    void Application::Close()
    {
        if (m_settings.KeepInTray && m_model.Active())
            HideToTray();
        else
            Quit();
    }

    LRESULT CALLBACK Application::WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        Application* self = nullptr;
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<Application*>(cs->lpCreateParams);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (self)
                self->m_hwnd = hwnd;
        }
        else
        {
            self = reinterpret_cast<Application*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self)
            return self->WndProc(hwnd, msg, wp, lp);
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT Application::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST))
            m_lastInputTick = ::GetTickCount64();

        if (msg == WM_MOUSELEAVE && m_openInBackground)
            return 0;

        if (msg == WM_MOUSEWHEEL && ImGui::GetCurrentContext())
        {
            m_wheel.Add(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA));
            return 0;
        }

        if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
            return 1;

        if (msg == ShowWindowMessage())
        {
            RestoreFromTray();
            return 0;
        }
        if (msg == m_taskbarCreated && m_trayIcon)
        {
            m_trayIcon = false;
            AddTrayIcon();
            return 0;
        }

        switch (msg)
        {
        case kTrayMessage:
            if (LOWORD(lp) == WM_LBUTTONUP)
                RestoreFromTray();
            else if (LOWORD(lp) == WM_RBUTTONUP)
                ShowTrayMenu();
            return 0;

        case WM_SIZE:
            if (wp != SIZE_MINIMIZED)
            {
                m_pendingWidth = LOWORD(lp);
                m_pendingHeight = HIWORD(lp);
                if (m_inSizeMove && !m_inFrame && m_renderTarget && ImGui::GetCurrentContext())
                {
                    ApplyPendingResize();
                    RenderFrame();
                }
            }
            return 0;

        case WM_ENTERSIZEMOVE:
            m_inSizeMove = true;
            ::SetTimer(hwnd, kSizeMoveTimer, kAnimationFrameMilliseconds, nullptr);
            return 0;

        case WM_EXITSIZEMOVE:
            m_inSizeMove = false;
            ::KillTimer(hwnd, kSizeMoveTimer);
            return 0;

        case WM_TIMER:
            if (wp == kSizeMoveTimer && !m_inFrame && m_renderTarget && ImGui::GetCurrentContext())
            {
                ApplyPendingResize();
                RenderFrame();
            }
            return 0;

        case WM_DPICHANGED:
        {
            RescaleForDpi(HIWORD(wp));
            const auto* suggested = reinterpret_cast<const RECT*>(lp);
            ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left, suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }

        case WM_GETMINMAXINFO:
        {
            const SIZE smallest = MinimumSize();
            auto* limits = reinterpret_cast<MINMAXINFO*>(lp);
            limits->ptMinTrackSize = { smallest.cx, smallest.cy };
            return 0;
        }

        case WM_NCCALCSIZE:
            if (wp)
            {
                if (::IsZoomed(hwnd))
                {
                    const UINT dpi = ::GetDpiForWindow(hwnd);
                    const int padding = ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
                    auto* sizes = reinterpret_cast<NCCALCSIZE_PARAMS*>(lp);
                    ::InflateRect(&sizes->rgrc[0], -(::GetSystemMetricsForDpi(SM_CXFRAME, dpi) + padding), -(::GetSystemMetricsForDpi(SM_CYFRAME, dpi) + padding));
                }
                return 0;
            }
            break;

        case WM_NCHITTEST:
        {
            POINT point{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ::ScreenToClient(hwnd, &point);

            if (!::IsZoomed(hwnd))
            {
                RECT client{};
                ::GetClientRect(hwnd, &client);
                const LONG edge = std::lround(kResizeEdge * WindowScale());
                const LONG corner = std::lround(kResizeCorner * WindowScale());
                const bool nearLeft = point.x < corner;
                const bool nearRight = point.x >= client.right - corner;
                const bool nearTop = point.y < corner;
                const bool nearBottom = point.y >= client.bottom - corner;
                if (point.y < edge)
                    return nearLeft ? HTTOPLEFT : nearRight ? HTTOPRIGHT : HTTOP;
                if (point.y >= client.bottom - edge)
                    return nearLeft ? HTBOTTOMLEFT : nearRight ? HTBOTTOMRIGHT : HTBOTTOM;
                if (point.x < edge)
                    return nearTop ? HTTOPLEFT : nearBottom ? HTBOTTOMLEFT : HTLEFT;
                if (point.x >= client.right - edge)
                    return nearTop ? HTTOPRIGHT : nearBottom ? HTBOTTOMRIGHT : HTRIGHT;
            }

            for (const RECT& area : m_dragAreas)
            {
                if (::PtInRect(&area, point))
                    return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_SYSCOMMAND:
            if ((wp & 0xFFF0) == SC_KEYMENU && lp == 0)
                return 0;
            break;

        case WM_SETTINGCHANGE:
            FollowSystemAnimations();
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
            ::ValidateRect(hwnd, nullptr);
            return 0;

        case WM_CLOSE:
            Close();
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
}
