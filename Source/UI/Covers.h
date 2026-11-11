#pragma once

#include "imgui.h"

#include <cstdint>
#include <filesystem>

namespace Idler::UI::Covers
{
    enum class Size
    {
        Thumb,
        Full
    };

    inline constexpr float kAspect = 460.0f / 215.0f;

    void Initialise(void* device);
    void Shutdown();

    void SetSteamRoot(const std::filesystem::path& root);
    void NewFrame();
    void SetPictureScale(float scale);

    ImTextureID Get(std::uint32_t appId, Size size);
    ImTextureID File(const std::filesystem::path& file, int width, int height);
    void RetryFiles();
    ImTextureID Avatar(std::uint64_t steamId);
    bool Loading();
}
