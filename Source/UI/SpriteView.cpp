#include "UI/SpriteView.h"

#include "Core/Creatures.h"
#include "UI/Theme.h"

#include <algorithm>
#include <string_view>

namespace Idler::UI
{
    namespace
    {
        constexpr std::size_t kColumnCount = static_cast<std::size_t>(CreatureSprites::kColumns);
        constexpr std::size_t kRowCount = static_cast<std::size_t>(CreatureSprites::kRows);
        constexpr std::size_t kLine = kColumnCount + 1;
        constexpr std::string_view kEvolvedPrefix = "evolved/";

        CreatureId IdOf(std::string_view key)
        {
            if (key.substr(0, kEvolvedPrefix.size()) == kEvolvedPrefix)
                key.remove_prefix(kEvolvedPrefix.size());
            return CreatureWithKey(key).value_or(CreatureId{});
        }
    }

    ImVec4 ColorOf(const SpriteFile::Color& color)
    {
        switch (color.Kind)
        {
        case SpriteFile::ColorKind::Rgb:
            return CreatureSprites::Rgb(color.Rgb);
        case SpriteFile::ColorKind::Accent:
            return ManifoldTheme::Accent;
        case SpriteFile::ColorKind::None:
            break;
        }
        return CreatureSprites::kNoColor;
    }

    SpriteView::SpriteView(const SpriteFile::Sprite& sprite)
    {
        const std::size_t frameCount = std::max<std::size_t>(1, sprite.Frames.size());
        m_cells.assign(frameCount * kRowCount * kLine, '.');
        m_frames.resize(frameCount);
        for (std::size_t frame = 0; frame < frameCount; ++frame)
        {
            for (std::size_t row = 0; row < kRowCount; ++row)
            {
                char* line = m_cells.data() + (frame * kRowCount + row) * kLine;
                line[kColumnCount] = '\0';
                if (frame < sprite.Frames.size() && row < sprite.Frames[frame].size())
                {
                    const std::string& source = sprite.Frames[frame][row];
                    for (std::size_t column = 0; column < kColumnCount && column < source.size(); ++column)
                        line[column] = SpriteFile::IsCell(source[column]) ? source[column] : '.';
                }
                m_frames[frame][row] = line;
            }
        }

        const int last = static_cast<int>(frameCount) - 1;
        for (const SpriteFile::Step& step : sprite.Steps)
            m_steps.push_back({ std::clamp(step.Frame, 0, last), step.Lift, step.Shift, step.Blink, std::max(1, step.Ticks) });
        if (m_steps.empty())
            m_steps.push_back({ 0, 0, 0, false, 1 });

        m_sprite = { IdOf(sprite.Key), m_frames.data(), static_cast<int>(m_frames.size()), m_steps.data(), static_cast<int>(m_steps.size()), std::clamp(sprite.Sleep, 0, last),
            ColorOf(sprite.Body), ColorOf(sprite.Second), ColorOf(sprite.Third), sprite.Facing, sprite.Floats, sprite.Swims.value_or(false), sprite.Stays };
    }
}
