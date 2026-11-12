#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::SpriteFile
{
    inline constexpr int kColumns = 16;
    inline constexpr int kRows = 14;
    inline constexpr int kMaxFrames = 8;
    inline constexpr int kMaxLift = 4;
    inline constexpr int kMaxShift = 2;
    inline constexpr double kTickSeconds = 0.125;
    inline constexpr std::string_view kCells = ".#o+adlcC*erywk";

    enum class ColorKind
    {
        None,
        Rgb,
        Accent
    };

    struct Color
    {
        ColorKind Kind = ColorKind::None;
        std::uint32_t Rgb = 0;

        bool operator==(const Color&) const = default;
    };

    struct Step
    {
        int Frame = 0;
        int Lift = 0;
        int Shift = 0;
        bool Blink = false;
        int Ticks = 1;

        bool operator==(const Step&) const = default;
    };

    using Frame = std::vector<std::string>;

    struct Sprite
    {
        std::string Key;
        std::string Id;
        Color Body;
        Color Second;
        Color Third;
        bool Floats = false;
        std::optional<bool> Swims;
        bool ObjectLike = false;
        int Sleep = 0;
        int Facing = 0;
        bool Stays = false;
        std::vector<Frame> Frames;
        std::vector<Step> Steps;

        bool operator==(const Sprite&) const = default;
    };

    std::optional<Sprite> Read(std::string_view text, std::string& problem);
    std::string Write(const Sprite& sprite);

    struct Findings
    {
        std::vector<std::string> Errors;
        std::vector<std::string> Warnings;
    };
    Findings Lint(const Sprite& sprite);

    bool IsCell(char cell);
    double LoopSeconds(const Sprite& sprite);
}
