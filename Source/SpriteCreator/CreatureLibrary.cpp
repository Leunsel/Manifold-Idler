#include "SpriteCreator/CreatureLibrary.h"

#include "Core/Text.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace Idler::SpriteCreator
{
    namespace
    {
        std::string NameOf(const std::optional<CreatureId>& id, bool evolved, const std::string& stem)
        {
            if (!id)
                return stem;
            if (!evolved)
                return InfoOf(*id).Name;
            for (const CreatureEvolution& evolution : kEvolutions)
            {
                if (evolution.Id == *id)
                    return evolution.Name;
            }
            return stem;
        }

        CreatureFile ReadCreature(const std::filesystem::path& path, bool evolved)
        {
            CreatureFile file;
            const std::string stem = WideToUtf8(path.stem().wstring());
            file.Key = evolved ? "evolved/" + stem : stem;
            file.Path = path;
            file.Evolved = evolved;
            file.Id = CreatureWithKey(stem);
            file.Name = NameOf(file.Id, evolved, stem);

            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                file.Problem = "the file cannot be opened";
                return file;
            }
            std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            file.Sprite = SpriteFile::Read(text, file.Problem);
            if (file.Sprite)
            {
                file.Findings = SpriteFile::Lint(*file.Sprite);
                file.View = std::make_unique<UI::SpriteView>(*file.Sprite);
            }
            return file;
        }
    }

    std::vector<CreatureFile> LoadCreatures(const std::filesystem::path& folder)
    {
        std::vector<CreatureFile> files;
        std::error_code ignored;
        for (const bool evolved : { false, true })
        {
            const std::filesystem::path place = evolved ? folder / "evolved" : folder;
            if (folder.empty() || !std::filesystem::is_directory(place, ignored))
                continue;
            for (std::filesystem::directory_iterator entry(place, ignored), end; !ignored && entry != end; entry.increment(ignored))
            {
                if (entry->is_regular_file(ignored) && entry->path().extension() == ".json")
                    files.push_back(ReadCreature(entry->path(), evolved));
            }
            ignored.clear();
        }
        std::stable_sort(files.begin(), files.end(), [](const CreatureFile& lhs, const CreatureFile& rhs)
            {
                if (lhs.Evolved != rhs.Evolved)
                    return !lhs.Evolved;
                if (lhs.Id.has_value() != rhs.Id.has_value())
                    return lhs.Id.has_value();
                if (lhs.Id && rhs.Id && *lhs.Id != *rhs.Id)
                    return *lhs.Id < *rhs.Id;
                return lhs.Key < rhs.Key;
            });
        return files;
    }

    Habit SpriteHabit(const SpriteFile::Sprite& sprite)
    {
        return sprite.Stays ? Habit::Stays : sprite.Swims.value_or(false) ? Habit::Swims : sprite.Floats ? Habit::Floats : Habit::Walks;
    }
}
