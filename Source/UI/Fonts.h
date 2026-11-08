#pragma once

struct ImFont;

namespace Idler::Fonts
{
    inline constexpr float kSize = 13.0f;

    bool Build(const char** error);

    ImFont* Bold();

    void AddFallbacks(unsigned needs);
}
