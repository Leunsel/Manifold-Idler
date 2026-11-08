#include "App/Splash.h"

#include "UI/CreatureSprites.h"
#include "UI/Theme.h"

#include <dwmapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <mutex>
#include <thread>

namespace Idler::Splash
{
    namespace
    {
        using UI::CreatureSprites::Frame;
        using UI::CreatureSprites::Sprite;
        using UI::CreatureSprites::Step;
        using UI::CreatureSprites::kColumns;
        using UI::CreatureSprites::kRows;

        constexpr wchar_t kClass[] = L"ManifoldIdlerSplash";
        constexpr int kWidth = 320;
        constexpr int kHeight = 200;
        constexpr int kCreaturePixel = 5;
        constexpr UINT_PTR kTimer = 1;
        constexpr UINT kFrameMilliseconds = 33;
        constexpr UINT kCloseMessage = WM_APP + 7;
        constexpr double kTickSeconds = 0.125;

        constexpr ImVec4 kRed = ImVec4(0.90f, 0.30f, 0.26f, 1.0f);
        constexpr ImVec4 kYellow = ImVec4(1.00f, 0.82f, 0.30f, 1.0f);
        constexpr ImVec4 kWhite = ImVec4(0.94f, 0.95f, 0.97f, 1.0f);
        constexpr ImVec4 kDark = ImVec4(0.27f, 0.27f, 0.33f, 1.0f);

        constexpr DWORD kUseImmersiveDarkMode = 20;
        constexpr DWORD kWindowCornerPreference = 33;
        constexpr DWORD kBorderColor = 34;
        constexpr DWORD kCornerRound = 2;

        COLORREF Color(ImVec4 color)
        {
            const auto channel = [](float value) { return static_cast<BYTE>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
            return RGB(channel(color.x), channel(color.y), channel(color.z));
        }

        COLORREF Mix(COLORREF from, COLORREF to, float amount)
        {
            const auto channel = [&](int a, int b) { return static_cast<BYTE>(std::lround(a + (b - a) * std::clamp(amount, 0.0f, 1.0f))); };
            return RGB(channel(GetRValue(from), GetRValue(to)), channel(GetGValue(from), GetGValue(to)), channel(GetBValue(from), GetBValue(to)));
        }

        ImVec4 Scaled(ImVec4 color, float brightness)
        {
            return ImVec4(std::min(1.0f, color.x * brightness), std::min(1.0f, color.y * brightness), std::min(1.0f, color.z * brightness), 1.0f);
        }

        ImVec4 Lightened(ImVec4 color)
        {
            return ImVec4(color.x + (1.0f - color.x) * 0.6f, color.y + (1.0f - color.y) * 0.6f, color.z + (1.0f - color.z) * 0.6f, 1.0f);
        }

        struct Palette
        {
            COLORREF Body, Shade, Light, Second, SecondShade, SecondLight, Third, ThirdShade, Eye, Red, Yellow, White, Dark;

            explicit Palette(const Sprite& sprite)
            {
                ImVec4 second = sprite.Second.w > 0.0f ? sprite.Second : sprite.Body;
                ImVec4 third = sprite.Third.w > 0.0f ? sprite.Third : second;
                Body = Color(Scaled(sprite.Body, 1.0f));
                Shade = Color(Scaled(sprite.Body, 0.62f));
                Light = Color(Lightened(sprite.Body));
                Second = Color(Scaled(second, 1.0f));
                SecondShade = Color(Scaled(second, 0.62f));
                SecondLight = Color(Lightened(second));
                Third = Color(Scaled(third, 1.0f));
                ThirdShade = Color(Scaled(third, 0.62f));
                Eye = Color(ManifoldTheme::Sidebar);
                Red = Color(kRed);
                Yellow = Color(kYellow);
                White = Color(kWhite);
                Dark = Color(kDark);
            }

            COLORREF Of(char cell, bool blink) const
            {
                switch (cell)
                {
                case '#': return Body;
                case 'o': return Shade;
                case '+': return Light;
                case 'a': return Second;
                case 'd': return SecondShade;
                case 'l': return SecondLight;
                case 'c': return Third;
                case 'C': return ThirdShade;
                case '*': return blink ? Shade : Eye;
                case 'e': return blink ? SecondShade : Eye;
                case 'r': return Red;
                case 'y': return Yellow;
                case 'w': return White;
                case 'k': return Dark;
                default: return CLR_INVALID;
                }
            }
        };

        const Sprite& SpriteOf(CreatureId id, bool evolved)
        {
            const auto index = static_cast<std::size_t>(id);
            if (evolved)
            {
                for (const Sprite& sprite : UI::CreatureSprites::kEvolvedSprites)
                {
                    if (sprite.Id == id)
                        return sprite;
                }
            }
            return UI::CreatureSprites::kSprites[std::min(index, std::size(UI::CreatureSprites::kSprites) - 1)];
        }

        const Step& StepAt(const Sprite& sprite, double seconds)
        {
            int total = 0;
            for (int i = 0; i < sprite.StepCount; ++i)
                total += sprite.Steps[i].Ticks;
            if (total <= 0)
                return sprite.Steps[0];
            auto tick = static_cast<long long>(std::floor(seconds / kTickSeconds)) % total;
            for (int i = 0; i < sprite.StepCount; ++i)
            {
                if (tick < sprite.Steps[i].Ticks)
                    return sprite.Steps[i];
                tick -= sprite.Steps[i].Ticks;
            }
            return sprite.Steps[0];
        }

        struct Request
        {
            HINSTANCE Instance = nullptr;
            RECT Around{};
            CreatureId Creature = CreatureId::Slime;
            bool Evolved = false;
            bool Animate = true;
            bool Activate = true;
        };

        std::thread g_thread;
        std::atomic<HWND> g_window{ nullptr };
        std::atomic<int> g_stage{ 0 };
        std::mutex g_lock;
        std::condition_variable g_started;
        bool g_startDone = false;

        struct Scene
        {
            const Sprite* Creature = nullptr;
            Palette Colors{ UI::CreatureSprites::kSprites[0] };
            bool Animate = true;
            ULONGLONG Start = 0;
            UINT Dpi = 96;
            HFONT Title = nullptr;
            HFONT TitleLight = nullptr;
            HFONT Status = nullptr;
        };
        Scene g_scene;

        int Scaled(int value)
        {
            return ::MulDiv(value, static_cast<int>(g_scene.Dpi), 96);
        }

        HFONT MakeFont(int pixels, int weight)
        {
            return ::CreateFontW(-Scaled(pixels), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
        }

        void FillBox(HDC dc, int left, int top, int right, int bottom, COLORREF color)
        {
            ::SetBkColor(dc, color);
            const RECT box{ left, top, right, bottom };
            ::ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &box, nullptr, 0, nullptr);
        }

        void Paint(HDC target, int width, int height)
        {
            HDC dc = ::CreateCompatibleDC(target);
            HBITMAP bitmap = ::CreateCompatibleBitmap(target, width, height);
            HGDIOBJ oldBitmap = ::SelectObject(dc, bitmap);

            const COLORREF page = Color(ManifoldTheme::Page);
            FillBox(dc, 0, 0, width, height, page);
            const double seconds = g_scene.Animate ? static_cast<double>(::GetTickCount64() - g_scene.Start) / 1000.0 : 0.0;

            const Sprite& sprite = *g_scene.Creature;
            const Step& step = StepAt(sprite, seconds);
            const int pixel = std::max(1, Scaled(kCreaturePixel));
            const int ground = Scaled(30) + pixel * kRows;
            const int left = (width - pixel * kColumns) / 2 + step.Shift * pixel;
            const int top = ground - pixel * kRows - step.Lift * pixel;
            const int shadowHalf = pixel * 5;
            FillBox(dc, width / 2 - shadowHalf, ground + pixel / 2, width / 2 + shadowHalf, ground + pixel / 2 + std::max(1, pixel / 2), Mix(page, RGB(0, 0, 0), 0.45f));
            const Frame& frame = sprite.Frames[std::clamp(step.FrameIndex, 0, sprite.FrameCount - 1)];
            for (int row = 0; row < kRows; ++row)
            {
                const char* line = frame[static_cast<std::size_t>(row)];
                int column = 0;
                while (column < kColumns)
                {
                    const COLORREF color = g_scene.Colors.Of(line[column], step.Blink);
                    int end = column + 1;
                    while (end < kColumns && g_scene.Colors.Of(line[end], step.Blink) == color)
                        ++end;
                    if (color != CLR_INVALID)
                        FillBox(dc, left + column * pixel, top + row * pixel, left + end * pixel, top + (row + 1) * pixel, color);
                    column = end;
                }
            }

            ::SetBkMode(dc, TRANSPARENT);
            constexpr wchar_t kFirst[] = L"Manifold";
            constexpr wchar_t kSecond[] = L" Idler";
            SIZE first{}, second{};
            HGDIOBJ oldFont = ::SelectObject(dc, g_scene.Title);
            ::GetTextExtentPoint32W(dc, kFirst, static_cast<int>(std::size(kFirst) - 1), &first);
            ::SelectObject(dc, g_scene.TitleLight);
            ::GetTextExtentPoint32W(dc, kSecond, static_cast<int>(std::size(kSecond) - 1), &second);
            const int titleX = (width - first.cx - second.cx) / 2;
            const int titleY = ground + Scaled(16);
            ::SelectObject(dc, g_scene.Title);
            ::SetTextColor(dc, Color(ManifoldTheme::Text));
            ::TextOutW(dc, titleX, titleY, kFirst, static_cast<int>(std::size(kFirst) - 1));
            ::SelectObject(dc, g_scene.TitleLight);
            ::SetTextColor(dc, Color(ManifoldTheme::Muted));
            ::TextOutW(dc, titleX + first.cx, titleY, kSecond, static_cast<int>(std::size(kSecond) - 1));

            const wchar_t* status = g_stage.load() == static_cast<int>(Stage::ReadingLibrary) ? L"Reading your library ..." : L"Starting ...";
            ::SelectObject(dc, g_scene.Status);
            SIZE statusSize{};
            ::GetTextExtentPoint32W(dc, status, static_cast<int>(std::wcslen(status)), &statusSize);
            ::TextOutW(dc, (width - statusSize.cx) / 2, titleY + first.cy + Scaled(6), status, static_cast<int>(std::wcslen(status)));
            ::SelectObject(dc, oldFont);

            const int trackLeft = width / 5;
            const int trackRight = width - width / 5;
            const int trackTop = height - Scaled(22);
            const int trackBottom = trackTop + std::max(1, Scaled(2));
            const COLORREF track = Mix(page, RGB(255, 255, 255), 0.07f);
            FillBox(dc, trackLeft, trackTop, trackRight, trackBottom, track);
            if (g_scene.Animate)
            {
                const double span = static_cast<double>(trackRight - trackLeft);
                const double band = span * 0.35;
                const double center = trackLeft - band + std::fmod(seconds / 1.6, 1.0) * (span + band * 2.0);
                const COLORREF accent = Color(ManifoldTheme::Accent);
                for (int x = trackLeft; x < trackRight; ++x)
                {
                    const double distance = std::abs(static_cast<double>(x) - center) / band;
                    if (distance < 1.0)
                        FillBox(dc, x, trackTop, x + 1, trackBottom, Mix(track, accent, static_cast<float>(0.5 + 0.5 * std::cos(distance * 3.14159265))));
                }
            }

            ::BitBlt(target, 0, 0, width, height, dc, 0, 0, SRCCOPY);
            ::SelectObject(dc, oldBitmap);
            ::DeleteObject(bitmap);
            ::DeleteDC(dc);
        }

        LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message)
            {
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT:
            {
                PAINTSTRUCT paint{};
                HDC dc = ::BeginPaint(hwnd, &paint);
                RECT client{};
                ::GetClientRect(hwnd, &client);
                Paint(dc, client.right, client.bottom);
                ::EndPaint(hwnd, &paint);
                return 0;
            }
            case WM_TIMER:
                ::InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            case kCloseMessage:
                ::DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                ::KillTimer(hwnd, kTimer);
                ::PostQuitMessage(0);
                return 0;
            default:
                return ::DefWindowProcW(hwnd, message, wParam, lParam);
            }
        }

        void Run(Request request)
        {
            WNDCLASSEXW windowClass{ sizeof(windowClass) };
            windowClass.style = CS_DROPSHADOW;
            windowClass.lpfnWndProc = &WndProc;
            windowClass.hInstance = request.Instance;
            windowClass.hCursor = ::LoadCursorW(nullptr, IDC_APPSTARTING);
            windowClass.lpszClassName = kClass;
            ::RegisterClassExW(&windowClass);

            const int middleX = (request.Around.left + request.Around.right) / 2;
            const int middleY = (request.Around.top + request.Around.bottom) / 2;
            HWND hwnd = ::CreateWindowExW(WS_EX_TOOLWINDOW, kClass, L"Manifold Idler", WS_POPUP, middleX, middleY, 1, 1, nullptr, nullptr, request.Instance, nullptr);
            if (hwnd)
            {
                g_scene.Creature = &SpriteOf(request.Creature, request.Evolved);
                g_scene.Colors = Palette(*g_scene.Creature);
                g_scene.Animate = request.Animate;
                g_scene.Start = ::GetTickCount64();
                g_scene.Dpi = std::max(96u, ::GetDpiForWindow(hwnd));
                g_scene.Title = MakeFont(18, FW_BOLD);
                g_scene.TitleLight = MakeFont(18, FW_NORMAL);
                g_scene.Status = MakeFont(12, FW_NORMAL);

                BOOL dark = TRUE;
                ::DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));
                DWORD corner = kCornerRound;
                ::DwmSetWindowAttribute(hwnd, kWindowCornerPreference, &corner, sizeof(corner));
                COLORREF border = Color(ManifoldTheme::Border);
                ::DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof(border));

                const int width = Scaled(kWidth);
                const int height = Scaled(kHeight);
                ::SetWindowPos(hwnd, nullptr, middleX - width / 2, middleY - height / 2, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
                if (request.Animate)
                    ::SetTimer(hwnd, kTimer, kFrameMilliseconds, nullptr);
                ::ShowWindow(hwnd, request.Activate ? SW_SHOWNORMAL : SW_SHOWNOACTIVATE);
                ::UpdateWindow(hwnd);
            }

            g_window = hwnd;
            {
                std::lock_guard lock(g_lock);
                g_startDone = true;
            }
            g_started.notify_all();
            if (!hwnd)
                return;

            MSG message{};
            while (::GetMessageW(&message, nullptr, 0, 0) > 0)
            {
                ::TranslateMessage(&message);
                ::DispatchMessageW(&message);
            }
            g_window = nullptr;
            for (HFONT font : { g_scene.Title, g_scene.TitleLight, g_scene.Status })
            {
                if (font)
                    ::DeleteObject(font);
            }
            g_scene = Scene{};
            ::UnregisterClassW(kClass, request.Instance);
        }
    }

    void Show(HINSTANCE instance, RECT around, CreatureId creature, bool evolved, bool animate, bool activate)
    {
        if (g_thread.joinable())
            return;
        {
            std::lock_guard lock(g_lock);
            g_startDone = false;
        }
        g_stage = static_cast<int>(Stage::Starting);
        g_thread = std::thread(Run, Request{ instance, around, creature, evolved, animate, activate });
        std::unique_lock lock(g_lock);
        g_started.wait(lock, [] { return g_startDone; });
    }

    void SetStage(Stage stage)
    {
        g_stage = static_cast<int>(stage);
    }

    void Close()
    {
        if (!g_thread.joinable())
            return;
        if (const HWND hwnd = g_window.load())
            ::PostMessageW(hwnd, kCloseMessage, 0, 0);
        g_thread.join();
    }
}
