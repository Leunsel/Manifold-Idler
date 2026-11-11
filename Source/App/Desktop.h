#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#ifdef _WIN32
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
#endif

namespace Idler::Desktop
{
    std::filesystem::path SettingsPath();

    void OpenSteam();
    void OpenStorePage(std::uint32_t appId);
    void OpenInLibrary(std::uint32_t appId);

    void SetSteamStatus(const wchar_t* status);

    bool KnowsInputIdle();
    double SecondsSinceInput();
    void KeepAwake(bool keep);

    void SetFakeShutdownFile(std::filesystem::path file);
    bool ShutDown();

    const char* StartAtLoginLabel();
    const char* StartAtLoginNote();
    bool StartsAtLogin();
    bool SetStartsAtLogin(bool start);

    bool CanMovePointer();
    bool PointerIn(void* window, float& x, float& y);
    void MovePointer(void* window, float x, float y);
    bool IsInFront(void* window);

#ifdef _WIN32
    bool SavePng(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, const std::filesystem::path& file);
#endif
}
