#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;

namespace Idler::Desktop
{
    std::filesystem::path SettingsPath();

    void OpenSteam();
    void OpenStorePage(std::uint32_t appId);
    void OpenInLibrary(std::uint32_t appId);

    void SetSteamStatus(const wchar_t* status);

    double SecondsSinceInput();
    void KeepAwake(bool keep);

    void SetFakeShutdownFile(std::filesystem::path file);
    bool ShutDown();

    bool StartsWithWindows();
    bool SetStartsWithWindows(bool start);

    bool PointerIn(void* window, float& x, float& y);
    void MovePointer(void* window, float x, float y);
    bool IsInFront(void* window);

    bool SavePng(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11Texture2D* backBuffer, const std::filesystem::path& file);
}
