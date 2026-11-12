#pragma once

#include "Core/SpriteFile.h"
#include "UI/CreatureSprites.h"

#include "imgui.h"

#include <vector>

namespace Idler::UI
{
    ImVec4 ColorOf(const SpriteFile::Color& color);

    class SpriteView
    {
    public:
        explicit SpriteView(const SpriteFile::Sprite& sprite);
        SpriteView(SpriteView&&) noexcept = default;
        SpriteView& operator=(SpriteView&&) noexcept = default;
        SpriteView(const SpriteView&) = delete;
        SpriteView& operator=(const SpriteView&) = delete;

        const CreatureSprites::Sprite& Get() const { return m_sprite; }

    private:
        std::vector<char> m_cells;
        std::vector<CreatureSprites::Frame> m_frames;
        std::vector<CreatureSprites::Step> m_steps;
        CreatureSprites::Sprite m_sprite{};
    };
}
