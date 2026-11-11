#pragma once

#include "imgui.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace Idler::UI::CoverPlatform
{
    struct Pixels
    {
        std::vector<std::uint8_t> Bgra;
        unsigned Width = 0;
        unsigned Height = 0;
    };

    void SetDevice(void* device);
    bool UploadsOnWorker();

    void BeginWorker();
    void EndWorker();
    bool Decode(const std::filesystem::path& file, unsigned width, unsigned height, float shape, Pixels& pixels);

    ImTextureID Upload(const Pixels& pixels);
    void Release(ImTextureID texture);
}
