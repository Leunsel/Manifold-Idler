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
    inline constexpr std::size_t kUndoDepth = 200;

    struct CreatureFile
    {
        std::string Key;
        std::string Name;
        std::filesystem::path Path;
        bool Evolved = false;
        std::optional<CreatureId> Id;
        std::optional<SpriteFile::Sprite> Sprite;
        std::optional<SpriteFile::Sprite> Saved;
        std::vector<SpriteFile::Sprite> Undo;
        std::vector<SpriteFile::Sprite> Redo;
        std::unique_ptr<UI::SpriteView> View;
        std::string Problem;
        SpriteFile::Findings Findings;

        bool Dirty() const { return Sprite && Saved && *Sprite != *Saved; }
    };

    std::vector<CreatureFile> LoadCreatures(const std::filesystem::path& folder);
    Habit SpriteHabit(const SpriteFile::Sprite& sprite);

    void Refresh(CreatureFile& file);
    void Remember(CreatureFile& file);
    void Forget(CreatureFile& file);
    bool StepBack(CreatureFile& file);
    bool StepForward(CreatureFile& file);
    bool SaveCreature(CreatureFile& file, std::string& problem);
}
