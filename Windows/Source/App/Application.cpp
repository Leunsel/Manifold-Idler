#include "App/Application.h"

#include "App/Desktop.h"
#include "App/Splash.h"
#include "App/WindowCommon.h"
#include "Core/Text.h"
#include "Resource.h"
#include "UI/Covers.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/ScreenMain.h"

#include <string>

#include <shellapi.h>

#include <wchar.h>

namespace Idler
{
    namespace
    {
        constexpr const wchar_t* kWindowClass = kCardFarmEnabled ? L"ManifoldIdlerAltWindow" : L"ManifoldIdlerWindow";
        constexpr const wchar_t* kWindowTitle = kCardFarmEnabled ? L"Manifold Idler (test)" : L"Manifold Idler";

        using namespace WindowCommon;

        constexpr UINT kTrayMessage = WM_APP + 1;
        constexpr UINT kTrayIconId = 1;
        constexpr UINT kMenuOpen = 1;
        constexpr UINT kMenuToggle = 2;
        constexpr UINT kMenuQuit = 3;

        Settings LoadSettings(const std::filesystem::path& path)
        {
            Settings settings;
            settings.Load(path);
            return settings;
        }

        WindowHost::Setup HostSetup()
        {
            WindowHost::Setup setup;
            setup.ClassName = kWindowClass;
            setup.Title = kWindowTitle;
            setup.MinimumWidth = kClientWidth;
            setup.MinimumHeight = kClientHeight;
            return setup;
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
        : m_settings(LoadSettings(settingsPath)), m_model(m_settings, std::move(settingsPath)), m_host(*this, HostSetup())
    {
    }

    Application::~Application()
    {
        Splash::Close();
        m_model.Cards().CloseWeb();
        RemoveTrayIcon();
        UI::Covers::Shutdown();
    }

    bool Application::CreateAppWindow(HINSTANCE instance, int showCommand)
    {
        m_instance = instance;
        m_taskbarCreated = ::RegisterWindowMessageW(L"TaskbarCreated");

        if (!m_host.Open(instance))
            return false;

        WindowPlacement placement;
        placement.X = m_settings.X;
        placement.Y = m_settings.Y;
        placement.Width = m_settings.Width;
        placement.Height = m_settings.Height;
        placement.Placed = m_settings.Placed;
        placement.Maximised = m_settings.Maximised;
        m_host.Place(placement);

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
            ::ShowWindow(m_host.Window(), showCommand);
            ::UpdateWindow(m_host.Window());
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
        m_host.ApplyPendingResize();
        m_host.RenderFrame();
        if (!::IsWindowVisible(m_host.Window()))
            ::ShowWindow(m_host.Window(), m_revealCommand);
        m_host.ApplyPendingResize();
        m_host.RenderFrame();
        Splash::Close();
        m_host.NoteInput();
    }

    void Application::RememberPlacement()
    {
        if (m_startMinimised)
            return;
        const std::optional<WindowPlacement> placement = m_host.Placement();
        if (!placement)
            return;
        m_settings.X = placement->X;
        m_settings.Y = placement->Y;
        m_settings.Width = placement->Width;
        m_settings.Height = placement->Height;
        m_settings.Maximised = placement->Maximised;
        m_settings.Placed = true;
    }

    void Application::BeforeFrame()
    {
        if (const unsigned needs = m_model.TakeFontNeeds(); needs != 0)
            Fonts::AddFallbacks(needs);
    }

    void Application::DrawFrame()
    {
        m_model.Update();
        UpdateTray();
        UI::Covers::SetSteamRoot(m_model.SteamStatus().Root);
        UI::Covers::SetPictureScale(m_host.WindowScale() * m_host.Zoom());
        UI::Covers::NewFrame();
        UI::IdleAnimation::BeginFrame();
        UI::DrawMain(*this);
    }

    void Application::AfterDraw()
    {
        CaptureIfDue();
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
        if (SUCCEEDED(m_host.SwapChain()->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer)
        {
            Desktop::SavePng(m_host.Device(), m_host.Context(), backBuffer, m_captureFile);
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
        if (!m_host.StartRendering())
            return false;

        UI::Covers::Initialise(m_host.Device());
        m_model.Cards().SetWindow(m_host.Window());

        if (m_startMinimised)
            HideToTray();
        m_host.NoteInput();
        m_running = true;
        return true;
    }

    int Application::Run()
    {
        while (m_running)
        {
            if (!m_host.PumpMessages())
                m_running = false;
            if (!m_running)
                break;

            if (m_revealPending)
            {
                m_model.Update();
                if (m_model.Scanning())
                    Splash::SetStage(Splash::Stage::ReadingLibrary);
                if (::IsWindowVisible(m_host.Window()) || ReadyToReveal())
                    Reveal();
                else
                    m_host.Wait(15);
                continue;
            }

            if (m_host.Hidden())
            {
                m_model.Update();
                UpdateTray();
                m_host.Wait(kHiddenUpdateMilliseconds);
                continue;
            }

            if (m_captureFile.empty() && !UI::IdleAnimation::NeedsFullRate() && m_host.InputAge() > kBusyMilliseconds)
                m_host.Wait(UI::IdleAnimation::NeedsFrames() ? kAnimationFrameMilliseconds : kIdleFrameMilliseconds);

            m_host.ApplyPendingResize();
            m_host.RenderFrame();
        }

        RememberPlacement();
        m_model.Finish();
        return 0;
    }

    void Application::AddTrayIcon()
    {
        if (m_trayIcon || !m_host.Window())
            return;
        NOTIFYICONDATAW data{ sizeof(data) };
        data.hWnd = m_host.Window();
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
        data.hWnd = m_host.Window();
        data.uID = kTrayIconId;
        ::Shell_NotifyIconW(NIM_DELETE, &data);
        m_trayIcon = false;
    }

    void Application::UpdateTray()
    {
        const bool finished = m_model.TakeQueueFinished();
        const bool cardDropped = m_model.TakeCardDropped();
        const std::string creatureNews = m_model.TakeCreatureNews();

        if (m_model.TakeWantsWindow() && !::IsWindowVisible(m_host.Window()))
            RestoreFromTray();

        if (::IsWindowVisible(m_host.Window()))
        {
            RemoveTrayIcon();
            return;
        }
        AddTrayIcon();
        if (!m_trayIcon)
            return;

        NOTIFYICONDATAW data{ sizeof(data) };
        data.hWnd = m_host.Window();
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
        ::SetForegroundWindow(m_host.Window());
        const UINT choice = ::TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, 0, m_host.Window(), nullptr);
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
        ::ShowWindow(m_host.Window(), SW_HIDE);
    }

    void Application::RestoreFromTray()
    {
        const bool maximise = m_startMinimised && m_settings.Maximised;
        m_startMinimised = false;
        ::ShowWindow(m_host.Window(), maximise ? SW_SHOWMAXIMIZED : ::IsIconic(m_host.Window()) ? SW_RESTORE : SW_SHOW);
        ::SetForegroundWindow(m_host.Window());
        m_host.NoteInput();
        RemoveTrayIcon();
    }

    void Application::Close()
    {
        if (m_settings.KeepInTray && m_model.Active())
            HideToTray();
        else
            Quit();
    }

    bool Application::HandleMessage(UINT message, WPARAM, LPARAM lParam, LRESULT& result)
    {
        result = 0;
        if (message == WM_MOUSELEAVE && m_openInBackground)
            return true;
        if (message == ShowWindowMessage())
        {
            RestoreFromTray();
            return true;
        }
        if (message == m_taskbarCreated && m_trayIcon)
        {
            m_trayIcon = false;
            AddTrayIcon();
            return true;
        }
        if (message == kTrayMessage)
        {
            if (LOWORD(lParam) == WM_LBUTTONUP)
                RestoreFromTray();
            else if (LOWORD(lParam) == WM_RBUTTONUP)
                ShowTrayMenu();
            return true;
        }
        return false;
    }
}
