#include "App/Desktop.h"

#include "Core/Clock.h"

#include <windows.h>
#include <d3d11.h>
#include <objbase.h>
#include <shellapi.h>
#include <wincodec.h>

#include <fstream>
#include <string>
#include <vector>

namespace Idler::Desktop
{
    namespace
    {
        constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
        constexpr wchar_t kRunValue[] = L"Manifold-Idler";

        std::filesystem::path g_fakeShutdownFile;

        std::wstring ExecutablePath()
        {
            std::wstring path(MAX_PATH, L'\0');
            DWORD length = 0;
            while ((length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))) == path.size())
                path.resize(path.size() * 2);
            path.resize(length);
            return path;
        }

        void OpenLink(const std::wstring& link)
        {
            ::ShellExecuteW(nullptr, L"open", link.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }

        template <typename T>
        void Release(T*& object)
        {
            if (object)
            {
                object->Release();
                object = nullptr;
            }
        }
    }

    std::filesystem::path SettingsPath()
    {
        return std::filesystem::path(ExecutablePath()).replace_filename(L"Manifold-Idler.ini");
    }

    void OpenSteam()
    {
        OpenLink(L"steam://open/main");
    }

    void OpenStorePage(std::uint32_t appId)
    {
        OpenLink(L"steam://store/" + std::to_wstring(appId));
    }

    void OpenInLibrary(std::uint32_t appId)
    {
        OpenLink(L"steam://nav/games/details/" + std::to_wstring(appId));
    }

    void SetSteamStatus(const wchar_t* status)
    {
        OpenLink(std::wstring(L"steam://friends/status/") + status);
    }

    double SecondsSinceInput()
    {
        LASTINPUTINFO info{ sizeof(info), 0 };
        if (!::GetLastInputInfo(&info))
            return 0.0;
        return static_cast<double>(static_cast<DWORD>(Clock::TickMilliseconds()) - info.dwTime) / 1000.0;
    }

    void KeepAwake(bool keep)
    {
        ::SetThreadExecutionState(keep ? ES_CONTINUOUS | ES_SYSTEM_REQUIRED : ES_CONTINUOUS);
    }

    void SetFakeShutdownFile(std::filesystem::path file)
    {
        g_fakeShutdownFile = std::move(file);
        std::ofstream marker(g_fakeShutdownFile, std::ios::binary | std::ios::trunc);
        marker << "dry run on\r\n";
    }

    bool ShutDown()
    {
        HANDLE token = nullptr;
        bool allowed = false;
        if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        {
            TOKEN_PRIVILEGES privileges{};
            privileges.PrivilegeCount = 1;
            privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            allowed = ::LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid) &&
                ::AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr) && ::GetLastError() == ERROR_SUCCESS;
            ::CloseHandle(token);
        }

        if (!g_fakeShutdownFile.empty())
        {
            std::ofstream file(g_fakeShutdownFile, std::ios::binary | std::ios::trunc);
            file << "shutdown, privilege " << (allowed ? "enabled" : "missing") << "\r\n";
            return true;
        }
        if (!allowed)
            return false;
        return ::InitiateShutdownW(nullptr, nullptr, 0, SHUTDOWN_POWEROFF, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED) == ERROR_SUCCESS;
    }

    bool PointerIn(void* window, float& x, float& y)
    {
        POINT point{};
        if (!window || !::GetCursorPos(&point) || !::ScreenToClient(static_cast<HWND>(window), &point))
            return false;
        x = static_cast<float>(point.x);
        y = static_cast<float>(point.y);
        return true;
    }

    void MovePointer(void* window, float x, float y)
    {
        POINT point{ static_cast<LONG>(x + 0.5f), static_cast<LONG>(y + 0.5f) };
        if (window && ::ClientToScreen(static_cast<HWND>(window), &point))
            ::SetCursorPos(point.x, point.y);
    }

    bool IsInFront(void* window)
    {
        return window && ::GetForegroundWindow() == static_cast<HWND>(window);
    }

    bool StartsWithWindows()
    {
        return ::RegGetValueW(HKEY_CURRENT_USER, kRunKey, kRunValue, RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    }

    bool SetStartsWithWindows(bool start)
    {
        HKEY key = nullptr;
        if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
            return false;

        LONG result = ERROR_SUCCESS;
        if (start)
        {
            const std::wstring command = L"\"" + ExecutablePath() + L"\" --minimised";
            result = ::RegSetValueExW(key, kRunValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
        }
        else
        {
            result = ::RegDeleteValueW(key, kRunValue);
            if (result == ERROR_FILE_NOT_FOUND)
                result = ERROR_SUCCESS;
        }
        ::RegCloseKey(key);
        return result == ERROR_SUCCESS;
    }

    bool SavePng(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, const std::filesystem::path& file)
    {
        D3D11_TEXTURE2D_DESC desc{};
        backBuffer->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ID3D11Texture2D* staging = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging)))
            return false;
        context->CopyResource(staging, backBuffer);

        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped)))
        {
            staging->Release();
            return false;
        }

        std::vector<BYTE> pixels(static_cast<std::size_t>(desc.Width) * desc.Height * 4);
        for (UINT y = 0; y < desc.Height; ++y)
        {
            const auto* source = static_cast<const BYTE*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch;
            BYTE* target = pixels.data() + static_cast<std::size_t>(y) * desc.Width * 4;
            for (UINT x = 0; x < desc.Width; ++x, source += 4, target += 4)
            {
                target[0] = source[2];
                target[1] = source[1];
                target[2] = source[0];
                target[3] = 255;
            }
        }
        context->Unmap(staging, 0);
        staging->Release();

        ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapEncoder* encoder = nullptr;
        IWICBitmapFrameEncode* frame = nullptr;
        HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr)) hr = factory->CreateStream(&stream);
        if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE);
        if (SUCCEEDED(hr)) hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
        if (SUCCEEDED(hr)) hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
        if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
        if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
        if (SUCCEEDED(hr)) hr = frame->SetSize(desc.Width, desc.Height);
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(hr)) hr = frame->SetPixelFormat(&format);
        if (SUCCEEDED(hr)) hr = frame->WritePixels(desc.Height, desc.Width * 4, static_cast<UINT>(pixels.size()), pixels.data());
        if (SUCCEEDED(hr)) hr = frame->Commit();
        if (SUCCEEDED(hr)) hr = encoder->Commit();
        Release(frame);
        Release(encoder);
        Release(stream);
        Release(factory);
        return SUCCEEDED(hr);
    }
}
