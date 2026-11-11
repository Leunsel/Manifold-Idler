#pragma once

#include <cstdint>
#include <vector>

namespace Idler::Gl
{
    bool Load();

    void Clear(int width, int height, float red, float green, float blue);
    bool ReadPixels(int width, int height, std::vector<std::uint8_t>& rgba);

    unsigned CreateTexture(const std::uint8_t* bgra, int width, int height);
    void DeleteTexture(unsigned texture);
}
