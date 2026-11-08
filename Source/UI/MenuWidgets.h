#pragma once

#include "UI/IdleAnimation.h"
#include "UI/Theme.h"

#include "imgui.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::UI
{
    inline constexpr float kSidebarWidth = 184.0f;

    void DrawWindowBackground(float rounding);
    bool SidebarItem(const char* icon, const char* label, bool current);

    void PageHeader(const char* icon, const char* title, std::string_view status, ImVec4 statusColor, float reserve);
    void BeginPageBody();
    void EndPageBody();

    void BeginCard(const char* title);
    void EndCard();
    void CardColumns(const char* id, const std::function<void()>& left, const std::function<void()>& right, float leftShare = 0.5f, float rightLimit = 0.0f);

    void BeginCardList(const char* id, float reserve);
    void EndCardList();
    float CardRowsHeight(int rows);

    float RowWidth();
    void BeginRow(const char* id);

    void ControlRow(const char* label, const char* tooltip, float width, const std::function<void()>& control,
        const char* warning = nullptr);
    bool ToggleRow(const char* label, const char* tooltip, bool& value);
    void InfoRow(const char* label, std::string_view value, ImVec4 color = ManifoldTheme::Text, ImTextureID image = ImTextureID{});

    struct RowTag
    {
        std::string Text;
        ImVec4 Color = ManifoldTheme::Muted;
        bool Animated = false;
        IdleAnimation::Mood Mood = IdleAnimation::Mood::Idling;
        std::uint32_t Seed = 0;
    };

    inline RowTag AnimatedTag(const char* meaning, IdleAnimation::Mood mood, std::uint32_t seed)
    {
        return { meaning, ManifoldTheme::Muted, true, mood, seed };
    }

    struct RowAction
    {
        const char* Icon = nullptr;
        const char* Tooltip = nullptr;
        bool Danger = false;
    };

    bool ListRow(const char* id, bool selected, const char* icon, ImVec4 iconColor, std::string_view name, ImVec4 nameColor,
        const std::vector<RowTag>& tags, ImTextureID image = ImTextureID{});
    float ListIconSize();

    struct GameRowOptions
    {
        float Progress = -1.0f;
        const RowAction* Action = nullptr;
        bool* ActionClicked = nullptr;
        bool Glow = false;
        std::uint32_t GlowSeed = 0;
    };

    bool GameRow(const char* id, bool selected, ImTextureID cover, std::string_view name, ImVec4 nameColor, std::string_view detail, ImVec4 detailColor,
        const std::vector<RowTag>& tags, const GameRowOptions& options = {});
    float GameRowHeight();

    struct CoverTileOptions
    {
        const RowAction* Action = nullptr;
        bool* ActionClicked = nullptr;
        bool ShowCreature = false;
        IdleAnimation::Mood Mood = IdleAnimation::Mood::Idling;
        std::uint32_t Seed = 0;
        const char* MarkTop = nullptr;
        const char* MarkBottom = nullptr;
        bool Detailed = false;
        std::string_view Detail;
        ImVec4 DetailColor = ManifoldTheme::Muted;
        float Progress = -1.0f;
        bool Glow = false;
    };

    bool CoverTile(const char* id, bool selected, ImTextureID cover, std::string_view name, float width, const CoverTileOptions& options);
    float CoverTileHeight(float width, bool detailed = false);

    bool IconButton(const char* label, float side, bool danger = false);

    bool ActionButton(const char* icon, std::string_view text, bool danger = false);
    void EndActionButtons();
    void ActionTooltip(const char* text);

    void DrawScrollShadows();
    void SetScrollSurface(ImVec4 color);

    bool DangerButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
    bool DangerMenuItem(const char* label);
    bool PositiveButton(const char* label, ImVec2 size = ImVec2(0.0f, 0.0f));
    bool PositiveIconButton(const char* label, float side);

    bool BeginDialog(const char* id, const char* icon, std::string_view title, ImVec2 size, std::string_view note = {}, bool* closed = nullptr, bool outsideCloses = true);

    void CoverStripRow(const char* id, const std::vector<ImTextureID>& covers, int more);
    void CoverRow(const char* id, ImTextureID cover, float maxHeight = 0.0f);
    void CoverPicture(ImTextureID cover, ImVec2 size);

    float Fade(ImGuiID key, bool on, float speed = 14.0f);
    void DrawIdleHighlight(ImDrawList* drawList, ImVec2 min, ImVec2 max, float strength, std::uint32_t seed = 0, ImVec4 color = ManifoldTheme::Accent);
    void DrawSheen(ImDrawList* drawList, ImVec2 min, ImVec2 max, float rounding, float center, float band, ImVec4 color);

    float ToggleSwitchWidth();
    bool ToggleSwitch(const char* id, bool& value);
    bool DrawCopyableText(std::string_view value, ImVec4 color = ManifoldTheme::Text, float maxWidth = 0.0f);
    void DrawCutText(ImDrawList* drawList, ImVec2 at, float right, ImVec4 color, std::string_view text);
    bool IconLink(const char* id, const char* icon, ImVec2 size, const char* tooltip,
        ImVec4 hoverColor = ImVec4(0.77f, 0.78f, 0.80f, 1.0f), ImVec4 restColor = ImVec4(0.44f, 0.45f, 0.49f, 1.0f));

    bool BeginContextMenu(const char* id, std::string_view title);

    void ShowTooltip(const std::function<void()>& content);
    void ItemTooltip(const char* text);
}
