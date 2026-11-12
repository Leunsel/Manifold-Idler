#pragma once

#include "App/WindowControls.h"
#include "SpriteCreator/CreatureLibrary.h"
#include "SpriteCreator/PaintCard.h"
#include "SpriteCreator/ScenePreview.h"
#include "SpriteCreator/TimelineCard.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Idler::SpriteCreator
{
    inline constexpr int kMinimumWidth = 1100;
    inline constexpr int kMinimumHeight = 700;
    inline constexpr int kStartWidth = 1240;
    inline constexpr int kStartHeight = 800;

    std::filesystem::path FindCreatureFolder(const std::filesystem::path& start);

    class Screen
    {
    public:
        void SetCreatureFolder(std::filesystem::path folder);
        const std::filesystem::path& CreatureFolder() const { return m_folder; }
        void Reload();
        void Select(std::string_view key);
        void SetBackdrop(Backdrop backdrop) { m_backdrop = backdrop; }
        void SetDaylight(float daylight) { m_daylight = daylight; }
        void SelectStep(int step) { m_timeline.Step = step; }

        void Draw(WindowControls& window);
        void RequestClose();
        bool TakeQuit();

    private:
        enum class Pending
        {
            None,
            Close,
            Reload
        };

        void DrawExplorer(float width, float height);
        void DrawCreatureRows(bool evolved);
        void DrawSceneRows();
        void DrawLight();
        void DrawPreview(float width, float height);
        void DrawProperties(float width, float height);
        void DrawUnsavedQuestion();
        void HandleKeys();
        void ChangeSelection(int index);
        void SaveSelected();
        bool SaveAll();
        void Say(std::string text, bool problem);
        std::size_t UnsavedCount() const;
        CreatureFile* Selected();
        const CreatureFile* Selected() const;
        const CreatureFile* Counterpart(const CreatureFile& file) const;

        std::filesystem::path m_folder;
        bool m_folderExists = false;
        std::vector<CreatureFile> m_files;
        int m_selected = -1;
        bool m_scrollToSelected = false;
        Backdrop m_backdrop = Backdrop::OwnStage;
        float m_daylight = 1.0f;
        std::array<char, 64> m_search{};
        PaintState m_paint;
        TimelineState m_timeline;
        Pending m_pending = Pending::None;
        bool m_quit = false;
        std::string m_message;
        bool m_messageIsProblem = false;
        double m_messageAt = -100.0;
    };
}
