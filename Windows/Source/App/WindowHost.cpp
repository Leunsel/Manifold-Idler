#include "App/WindowHost.h"

#include "App/WindowCommon.h"
#include "Core/Errors.h"
#include "Core/Text.h"
#include "Resource.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/Theme.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include <dwmapi.h>
#include <dxgi1_3.h>
#include <windowsx.h>

#include <stdio.h>
#include <wchar.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Idler
{
    namespace
    {
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

        constexpr UINT_PTR kSizeMoveTimer = 1;

        void FollowSystemAnimations()
        {
            BOOL on = TRUE;
            if (!::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0))
                on = TRUE;
            UI::IdleAnimation::SetEnabled(on != FALSE);
        }
    }

    WindowHost::WindowHost(Client& client, Setup setup) : m_client(client), m_setup(std::move(setup))
    {
    }

    WindowHost::~WindowHost()
    {
        m_closing = true;
        if (ImGui::GetCurrentContext())
        {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        DestroyDeviceAndSwapChain();
        if (m_hwnd)
            ::DestroyWindow(m_hwnd);
        if (m_registered)
            ::UnregisterClassW(m_setup.ClassName, m_instance);
        delete static_cast<ImGuiStyle*>(m_baseStyle);
    }

    bool WindowHost::Open(HINSTANCE instance)
    {
        m_instance = instance;

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc = &WindowHost::WndProcThunk;
        wc.hInstance = instance;
        wc.hIcon = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), 0));
        wc.hIconSm = static_cast<HICON>(::LoadImageW(instance, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
            ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0));
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = m_setup.ClassName;
        if (!::RegisterClassExW(&wc))
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::WindowClassFailed);
            return false;
        }
        m_registered = true;

        m_hwnd = ::CreateWindowExW(0, m_setup.ClassName, m_setup.Title, kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT,
            m_setup.MinimumWidth, m_setup.MinimumHeight, nullptr, nullptr, instance, this);
        if (!m_hwnd)
        {
            PutError(m_error, ARRAYSIZE(m_error), ErrorCode::WindowFailed);
            return false;
        }

        m_dpi = ::GetDpiForWindow(m_hwnd);
        return true;
    }

    SIZE WindowHost::MinimumSize() const
    {
        const float scale = WindowScale();
        return { std::lround(m_setup.MinimumWidth * scale), std::lround(m_setup.MinimumHeight * scale) };
    }

    void WindowHost::Place(const WindowPlacement& placement)
    {
        const float scale = WindowScale();
        const SIZE smallest = MinimumSize();
        SIZE size{ (std::max)(smallest.cx, std::lround(placement.Width * scale)), (std::max)(smallest.cy, std::lround(placement.Height * scale)) };
        int x = 0;
        int y = 0;
        bool placed = false;

        const RECT saved{ placement.X, placement.Y, placement.X + size.cx, placement.Y + size.cy };
        if (placement.Placed && ::MonitorFromRect(&saved, MONITOR_DEFAULTTONULL))
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
        ApplyWindowChrome();
    }

    std::optional<WindowPlacement> WindowHost::Placement() const
    {
        if (!m_hwnd)
            return std::nullopt;

        WINDOWPLACEMENT placement{ sizeof(placement) };
        if (!::GetWindowPlacement(m_hwnd, &placement))
            return std::nullopt;

        RECT rect = placement.rcNormalPosition;
        if (!::IsIconic(m_hwnd) && !::IsZoomed(m_hwnd) && ::IsWindowVisible(m_hwnd))
        {
            if (!::GetWindowRect(m_hwnd, &rect))
                return std::nullopt;
        }
        else
        {
            MONITORINFO info = { sizeof(info), {}, {}, 0 };
            if (::GetMonitorInfoW(::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST), &info))
                ::OffsetRect(&rect, info.rcWork.left - info.rcMonitor.left, info.rcWork.top - info.rcMonitor.top);
        }

        const float scale = WindowScale();
        WindowPlacement result;
        result.X = rect.left;
        result.Y = rect.top;
        result.Width = std::lround((rect.right - rect.left) / scale);
        result.Height = std::lround((rect.bottom - rect.top) / scale);
        result.Maximised = ::IsZoomed(m_hwnd) || (::IsIconic(m_hwnd) && (placement.flags & WPF_RESTORETOMAXIMIZED));
        result.Placed = true;
        return result;
    }

    void WindowHost::ApplyWindowChrome()
    {
        const BOOL dark = TRUE;
        ::DwmSetWindowAttribute(m_hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));

        const DWORD corner = kCornerRound;
        m_roundedCorners = SUCCEEDED(::DwmSetWindowAttribute(m_hwnd, kWindowCornerPreference, &corner, sizeof(corner)));
        const COLORREF border = kColorNone;
        ::DwmSetWindowAttribute(m_hwnd, kBorderColor, &border, sizeof(border));
    }

    float WindowHost::CornerRadius() const
    {
        return m_roundedCorners && !Maximised() ? kCornerRadius * WindowScale() : 0.0f;
    }

    void WindowHost::AddDragArea(float left, float top, float right, float bottom)
    {
        m_dragAreas.push_back(RECT{ std::lround(left), std::lround(top), std::lround(right), std::lround(bottom) });
    }

    bool WindowHost::CreateDeviceAndSwapChain()
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

    bool WindowHost::CreateRenderTarget()
    {
        ID3D11Texture2D* backBuffer = nullptr;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (FAILED(hr) || !backBuffer)
            return false;
        hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_renderTarget);
        backBuffer->Release();
        return SUCCEEDED(hr);
    }

    void WindowHost::DestroyRenderTarget()
    {
        if (m_renderTarget)
        {
            m_renderTarget->Release();
            m_renderTarget = nullptr;
        }
    }

    void WindowHost::DestroyDeviceAndSwapChain()
    {
        DestroyRenderTarget();
        if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
        if (m_context) { m_context->Release(); m_context = nullptr; }
        if (m_device) { m_device->Release(); m_device = nullptr; }
    }

    float WindowHost::WindowScale() const
    {
        return (m_scaleOverride > 0.0f ? m_scaleOverride : static_cast<float>(m_dpi) / 96.0f);
    }

    void WindowHost::UpdateZoom(UINT width, UINT height)
    {
        const float zoom = ZoomFor(static_cast<float>(width), static_cast<float>(height), WindowScale(), static_cast<float>(m_setup.MinimumWidth),
            static_cast<float>(m_setup.MinimumHeight));
        if (zoom == m_zoom)
            return;
        m_zoom = zoom;
        RescaleForDpi(m_dpi);
    }

    void WindowHost::RescaleForDpi(UINT dpi)
    {
        m_dpi = dpi;
        ScaleStyle(*static_cast<const ImGuiStyle*>(m_baseStyle), WindowScale() * m_zoom);
    }

    void WindowHost::ApplyPendingResize()
    {
        if (m_pendingWidth == 0 || m_pendingHeight == 0 || !m_swapChain)
            return;
        DestroyRenderTarget();
        m_swapChain->ResizeBuffers(0, m_pendingWidth, m_pendingHeight, DXGI_FORMAT_UNKNOWN, 0);
        UpdateZoom(m_pendingWidth, m_pendingHeight);
        m_pendingWidth = m_pendingHeight = 0;
        CreateRenderTarget();
    }

    void WindowHost::RenderFrame()
    {
        if (m_inFrame)
            return;
        m_inFrame = true;
        struct FrameDone
        {
            bool& Flag;
            ~FrameDone() { Flag = false; }
        } frameDone{ m_inFrame };

        m_client.BeforeFrame();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (const float wheel = m_wheel.Take(ImGui::GetIO().DeltaTime, UI::IdleAnimation::Enabled()); wheel != 0.0f)
            ImGui::GetIO().AddMouseWheelEvent(0.0f, wheel);
        ImGui::GetIO().ConfigInputTrickleEventQueue = ClicksOrKeysQueued();
        ImGui::NewFrame();

        m_dragAreas.clear();
        m_client.DrawFrame();

        ImGui::Render();

        const ImVec4 clear = ManifoldTheme::Page;
        const float rgba[4] = { clear.x, clear.y, clear.z, 1.0f };
        m_context->OMSetRenderTargets(1, &m_renderTarget, nullptr);
        m_context->ClearRenderTargetView(m_renderTarget, rgba);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        m_client.AfterDraw();

        const HRESULT hr = m_swapChain->Present(1, 0);
        m_occluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    bool WindowHost::StartRendering()
    {
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
        FollowSystemAnimations();
        return true;
    }

    bool WindowHost::PumpMessages()
    {
        bool quit = false;
        MSG msg = {};
        while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                quit = true;
        }
        return !quit;
    }

    bool WindowHost::Hidden()
    {
        if (!::IsWindowVisible(m_hwnd) || ::IsIconic(m_hwnd) || (m_occluded && m_swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED))
            return true;
        m_occluded = false;
        return false;
    }

    void WindowHost::Wait(unsigned milliseconds) const
    {
        ::MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }

    void WindowHost::NoteInput()
    {
        m_lastInputTick = ::GetTickCount64();
    }

    unsigned long long WindowHost::InputAge() const
    {
        return ::GetTickCount64() - m_lastInputTick;
    }

    LRESULT CALLBACK WindowHost::WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        WindowHost* self = nullptr;
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            self = static_cast<WindowHost*>(cs->lpCreateParams);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            if (self)
                self->m_hwnd = hwnd;
        }
        else
        {
            self = reinterpret_cast<WindowHost*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (self)
            return self->WndProc(hwnd, msg, wp, lp);
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }

    LRESULT WindowHost::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if ((msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || (msg >= WM_KEYFIRST && msg <= WM_KEYLAST))
            m_lastInputTick = ::GetTickCount64();

        if (!m_closing)
        {
            LRESULT handled = 0;
            if (m_client.HandleMessage(msg, wp, lp, handled))
                return handled;
        }

        if (msg == WM_MOUSEWHEEL && ImGui::GetCurrentContext())
        {
            m_wheel.Add(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wp)) / static_cast<float>(WHEEL_DELTA));
            return 0;
        }

        if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
            return 1;

        switch (msg)
        {
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
            if (!m_closing)
                m_client.CloseRequested();
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
