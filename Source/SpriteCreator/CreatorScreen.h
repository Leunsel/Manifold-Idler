#pragma once

#include "App/WindowControls.h"

#include <filesystem>
#include <utility>

namespace Idler::SpriteCreator
{
    inline constexpr int kMinimumWidth = 1040;
    inline constexpr int kMinimumHeight = 680;
    inline constexpr int kStartWidth = 1180;
    inline constexpr int kStartHeight = 760;

    std::filesystem::path FindCreatureFolder(const std::filesystem::path& start);

    class Screen
    {
    public:
        void SetCreatureFolder(std::filesystem::path folder);
        const std::filesystem::path& CreatureFolder() const { return m_folder; }

        void Draw(WindowControls& window);

    private:
        std::filesystem::path m_folder;
        bool m_folderExists = false;
    };
}
