#pragma once

#include "Core/Creatures.h"
#include "Core/SpriteFile.h"
#include "Core/Terrarium.h"
#include "UI/SpriteView.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Idler::SpriteCreator
{
    struct CreatureFile
    {
        std::string Key;
        std::string Name;
        std::filesystem::path Path;
        bool Evolved = false;
        std::optional<CreatureId> Id;
        std::optional<SpriteFile::Sprite> Sprite;
        std::unique_ptr<UI::SpriteView> View;
        std::string Problem;
        SpriteFile::Findings Findings;
    };

    std::vector<CreatureFile> LoadCreatures(const std::filesystem::path& folder);
    Habit SpriteHabit(const SpriteFile::Sprite& sprite);
}
