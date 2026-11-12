#include "SpriteCreator/CreatorScreen.h"

#include "Core/SpriteFile.h"
#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"
#include "UI/Fonts.h"
#include "UI/IdleAnimation.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <system_error>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;
        using UI::PxWhole;
        namespace Animation = UI::IdleAnimation;

        constexpr ImVec4 kRunningRow = ImVec4(0.100f, 0.205f, 0.176f, 1.0f);

        struct Light
        {
            const char* Name;
            float Daylight;
        };
        constexpr Light kLights[] = { { "Day", 1.0f }, { "Dawn", 0.55f }, { "Dusk", 0.45f }, { "Night", 0.0f } };

        ImU32 Color(ImVec4 color)
        {
            return ImGui::GetColorU32(color);
        }

        std::uint32_t SeedOf(int index)
        {
            return static_cast<std::uint32_t>(index) * 7919u + 13u;
        }

        bool WindowButton(const char* id, const char* icon, float side, ImVec4 hoverColor)
        {
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const bool clicked = ImGui::InvisibleButton(id, ImVec2(side, side));
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (hovered)
                drawList->AddRectFilled(pos, ImVec2(pos.x + side, pos.y + side), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.07f)), Px(4.0f));
            const ImVec2 size = ImGui::CalcTextSize(icon);
            drawList->AddText(ImVec2(std::round(pos.x + (side - size.x) * 0.5f), std::round(pos.y + (side - size.y) * 0.5f)),
                Color(hovered ? hoverColor : ManifoldTheme::Label), icon);
            return clicked;
        }

        void BoldText(ImDrawList* drawList, ImVec2 at, ImVec4 color, const char* text)
        {
            ImFont* bold = Fonts::Bold();
            if (bold)
                drawList->AddText(bold, ImGui::GetFontSize(), at, Color(color), text);
            else
                drawList->AddText(at, Color(color), text);
        }

        float BoldWidth(const char* text)
        {
            ImFont* bold = Fonts::Bold();
            return bold ? bold->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, text).x : ImGui::CalcTextSize(text).x;
        }

        void TitleBar(WindowControls& window, const std::filesystem::path& folder, bool found, std::size_t count)
        {
            const ImVec2 origin = ImGui::GetWindowPos();
            const float width = ImGui::GetWindowWidth();
            const float height = std::round(Px(40.0f));
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), Color(ManifoldTheme::Sidebar), window.CornerRadius(), ImDrawFlags_RoundCornersTop);
            drawList->AddLine(ImVec2(origin.x, origin.y + height - 1.0f), ImVec2(origin.x + width, origin.y + height - 1.0f), Color(ManifoldTheme::RowSeparator));

            const float side = std::round(Px(26.0f));
            const float gap = Px(2.0f);
            const float buttonsX = std::round(origin.x + width - Px(10.0f) - side * 3.0f - gap * 2.0f);
            const float textY = std::round(origin.y + (height - ImGui::GetTextLineHeight()) * 0.5f);

            float x = origin.x + Px(16.0f);
            drawList->AddText(ImVec2(x, textY), Color(ManifoldTheme::Accent), ICON_FA_CUBE);
            x += ImGui::CalcTextSize(ICON_FA_CUBE).x + Px(9.0f);
            constexpr const char* kTitle = "Sprite Creator";
            BoldText(drawList, ImVec2(x, textY), ManifoldTheme::Text, kTitle);
            x += BoldWidth(kTitle);

            std::string status;
            if (found)
                status = WideToUtf8(folder.wstring()) + "  \xC2\xB7  " + std::to_string(count) + (count == 1 ? " file" : " files");
            else if (folder.empty())
                status = "No creature folder found. Start it with --creatures=<folder>.";
            else
                status = "The creature folder " + WideToUtf8(folder.wstring()) + " does not exist.";
            const float statusLeft = x + Px(18.0f);
            const float statusRight = buttonsX - Px(16.0f);
            if (statusRight > statusLeft)
            {
                drawList->PushClipRect(ImVec2(statusLeft, origin.y), ImVec2(statusRight, origin.y + height), true);
                drawList->AddText(ImVec2(statusLeft, textY), Color(found ? ManifoldTheme::Muted : ManifoldTheme::Warning), status.c_str());
                drawList->PopClipRect();
            }

            const float buttonsY = std::round(origin.y + (height - side) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(buttonsX, buttonsY));
            if (WindowButton("##minimise", ICON_FA_MINUS, side, ManifoldTheme::Text))
                window.Minimise();
            ImGui::SameLine(0.0f, gap);
            if (WindowButton("##maximise", window.Maximised() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, side, ManifoldTheme::Text))
                window.ToggleMaximised();
            ImGui::SameLine(0.0f, gap);
            if (WindowButton("##close", ICON_FA_TIMES, side, ManifoldTheme::Error))
                window.Close();

            window.AddDragArea(origin.x, origin.y, buttonsX, origin.y + height);
            window.AddDragArea(buttonsX, origin.y, origin.x + width, buttonsY);
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
        }

        void BeginCard(const char* id, const char* icon, const char* title, ImVec2 size, const char* note = nullptr)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ManifoldTheme::Card);
            ImGui::PushStyleColor(ImGuiCol_Border, ManifoldTheme::CardBorder);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, Px(6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(14.0f, 12.0f));
            ImGui::BeginChild(id, size, ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar(3);
            ImGui::PopStyleColor(2);

            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
            ImGui::TextUnformatted(icon);
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, Px(8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Label);
            ImGui::TextUnformatted(title);
            ImGui::PopStyleColor();
            if (note && *note)
            {
                const float right = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
                const float noteWidth = ImGui::CalcTextSize(note).x;
                ImGui::SameLine(right - noteWidth);
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
                ImGui::TextUnformatted(note);
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
        }

        void EndCard()
        {
            ImGui::EndChild();
        }

        void SectionHeader(const char* title, std::size_t count)
        {
            ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
            const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(title);
            if (count > 0)
            {
                const std::string text = std::to_string(count);
                ImGui::SameLine(right - ImGui::CalcTextSize(text.c_str()).x);
                ImGui::TextUnformatted(text.c_str());
            }
            ImGui::PopStyleColor();
        }

        void DrawGrid(const UI::CreatureSprites::Sprite* sprite, int frame)
        {
            const ImVec2 room = ImGui::GetContentRegionAvail();
            const float cell = std::floor(std::min(room.x / SpriteFile::kColumns, room.y / SpriteFile::kRows));
            if (cell < 2.0f)
                return;
            const ImVec2 size(cell * SpriteFile::kColumns, cell * SpriteFile::kRows);
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 min(std::round(cursor.x + (room.x - size.x) * 0.5f), std::round(cursor.y + (room.y - size.y) * 0.5f));
            const ImVec2 max(min.x + size.x, min.y + size.y);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 light = Color(ImVec4(0.125f, 0.131f, 0.150f, 1.0f));
            const ImU32 dark = Color(ImVec4(0.100f, 0.105f, 0.121f, 1.0f));
            for (int row = 0; row < SpriteFile::kRows; ++row)
            {
                for (int column = 0; column < SpriteFile::kColumns; ++column)
                {
                    const ImVec2 cellMin(min.x + cell * static_cast<float>(column), min.y + cell * static_cast<float>(row));
                    drawList->AddRectFilled(cellMin, ImVec2(cellMin.x + cell, cellMin.y + cell), (row + column) % 2 == 0 ? light : dark);
                }
            }
            if (sprite)
                Animation::DrawSpriteFrame(drawList, min, *sprite, frame, cell, false, Animation::Tint::Colors);
            for (int column = 0; column <= SpriteFile::kColumns; ++column)
            {
                const float x = min.x + cell * static_cast<float>(column);
                drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, column % 4 == 0 ? 0.09f : 0.04f)));
            }
            for (int row = 0; row <= SpriteFile::kRows; ++row)
            {
                const float y = min.y + cell * static_cast<float>(row);
                drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, row == SpriteFile::kRows - 1 ? 0.09f : 0.04f)));
            }
            ImGui::Dummy(room);
        }

        void FactRow(const char* label, const std::string& value, ImVec4 color = ManifoldTheme::Text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine(std::round(Px(84.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(value.c_str());
            ImGui::PopStyleColor();
        }

        std::string ColorName(const SpriteFile::Color& color)
        {
            if (color.Kind == SpriteFile::ColorKind::None)
                return "none";
            if (color.Kind == SpriteFile::ColorKind::Accent)
                return "accent";
            char text[16];
            std::snprintf(text, sizeof(text), "#%06X", static_cast<unsigned>(color.Rgb & 0xFFFFFF));
            return text;
        }

        void ColorRow(const char* label, const SpriteFile::Color& color)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine(std::round(Px(84.0f)));
            const float side = ImGui::GetTextLineHeight();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            if (color.Kind != SpriteFile::ColorKind::None)
                drawList->AddRectFilled(at, ImVec2(at.x + side, at.y + side), Color(UI::ColorOf(color)), Px(3.0f));
            else
                drawList->AddRect(at, ImVec2(at.x + side, at.y + side), Color(ManifoldTheme::Border), Px(3.0f));
            ImGui::Dummy(ImVec2(side, side));
            ImGui::SameLine(0.0f, Px(8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, color.Kind == SpriteFile::ColorKind::None ? ManifoldTheme::Muted : ManifoldTheme::Text);
            ImGui::TextUnformatted(ColorName(color).c_str());
            ImGui::PopStyleColor();
        }

        const char* HabitName(Habit habit)
        {
            switch (habit)
            {
            case Habit::Floats:
                return "floats";
            case Habit::Swims:
                return "swims in the lake";
            case Habit::Stays:
                return "stays in one place";
            case Habit::Walks:
                break;
            }
            return "walks";
        }

        const char* BackdropIcon(Backdrop backdrop)
        {
            switch (backdrop)
            {
            case Backdrop::OwnStage:
                return ICON_FA_THEATER_MASKS;
            case Backdrop::Meadow:
                return ICON_FA_TREE;
            case Backdrop::Clouds:
                return ICON_FA_CLOUD;
            case Backdrop::Underwater:
                return ICON_FA_WATER;
            case Backdrop::Spring:
                return ICON_FA_SEEDLING;
            case Backdrop::Summer:
                return ICON_FA_SUN;
            case Backdrop::Autumn:
                return ICON_FA_LEAF;
            case Backdrop::Winter:
                return ICON_FA_SNOWFLAKE;
            case Backdrop::Plain:
                break;
            }
            return ICON_FA_SQUARE;
        }

        void SmallPreview(const char* label, ImVec4 background, const UI::CreatureSprites::Sprite& sprite, float pixel, Animation::Mood mood, Animation::Look look,
            std::uint32_t seed, float tileWidth)
        {
            const float tileHeight = std::round(Px(3.0f)) * static_cast<float>(UI::CreatureSprites::kRows) + Px(14.0f);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 boxMax(pos.x + tileWidth, pos.y + tileHeight);
            drawList->AddRectFilled(pos, boxMax, Color(background), Px(4.0f));
            const ImVec2 size = Animation::SizeAt(pixel);
            const ImVec2 at(std::round(pos.x + (tileWidth - size.x) * 0.5f), std::round(boxMax.y - Px(5.0f) - size.y));
            drawList->PushClipRect(pos, boxMax, true);
            Animation::DrawSprite(drawList, at, sprite, mood, seed, pixel, look);
            drawList->PopClipRect();
            const ImVec2 labelSize = ImGui::CalcTextSize(label);
            drawList->AddText(ImVec2(std::round(pos.x + (tileWidth - labelSize.x) * 0.5f), boxMax.y + Px(4.0f)), Color(ManifoldTheme::Muted), label);
            ImGui::Dummy(ImVec2(tileWidth, tileHeight + labelSize.y + Px(8.0f)));
        }
    }

    void Screen::SetCreatureFolder(std::filesystem::path folder)
    {
        std::error_code ignored;
        m_folderExists = !folder.empty() && std::filesystem::is_directory(folder, ignored);
        m_folder = std::move(folder);
        Reload();
    }

    void Screen::Reload()
    {
        const std::string selectedKey = Selected() ? Selected()->Key : std::string();
        m_files = m_folderExists ? LoadCreatures(m_folder) : std::vector<CreatureFile>{};
        m_selected = m_files.empty() ? -1 : 0;
        for (std::size_t index = 0; index < m_files.size(); ++index)
        {
            if (m_files[index].Key == selectedKey)
                m_selected = static_cast<int>(index);
        }
    }

    void Screen::Select(std::string_view key)
    {
        for (std::size_t index = 0; index < m_files.size(); ++index)
        {
            if (m_files[index].Key == key)
            {
                m_selected = static_cast<int>(index);
                m_scrollToSelected = true;
            }
        }
    }

    const CreatureFile* Screen::Selected() const
    {
        if (m_selected < 0 || m_selected >= static_cast<int>(m_files.size()))
            return nullptr;
        return &m_files[static_cast<std::size_t>(m_selected)];
    }

    const CreatureFile* Screen::Counterpart(const CreatureFile& file) const
    {
        if (!file.Id)
            return nullptr;
        for (const CreatureFile& other : m_files)
        {
            if (&other != &file && other.Id == file.Id && other.Evolved != file.Evolved)
                return &other;
        }
        return nullptr;
    }

    std::filesystem::path FindCreatureFolder(const std::filesystem::path& start)
    {
        std::error_code ignored;
        if (start.empty())
            return {};
        if (std::filesystem::is_directory(start / "creatures", ignored))
            return start / "creatures";
        for (std::filesystem::path folder = start; !folder.empty(); folder = folder.parent_path())
        {
            const std::filesystem::path candidate = folder / "Tools" / "CreatureForge" / "creatures";
            if (std::filesystem::is_directory(candidate, ignored))
                return candidate;
            if (folder == folder.parent_path())
                break;
        }
        return {};
    }

    void Screen::DrawCreatureRows(bool evolved)
    {
        const std::string filter = FoldCase(Trim(std::string_view(m_search.data())));
        const float pixel = Animation::PixelSize();
        const float rowHeight = std::round(pixel * static_cast<float>(UI::CreatureSprites::kRows) + Px(10.0f));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (std::size_t index = 0; index < m_files.size(); ++index)
        {
            const CreatureFile& file = m_files[index];
            if (file.Evolved != evolved)
                continue;
            if (!filter.empty() && FoldCase(file.Name).find(filter) == std::string::npos && FoldCase(file.Key).find(filter) == std::string::npos)
                continue;

            ImGui::PushID(static_cast<int>(index));
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##row", ImVec2(width, rowHeight)))
                m_selected = static_cast<int>(index);
            if (m_scrollToSelected && m_selected == static_cast<int>(index))
            {
                ImGui::SetScrollHereY(0.3f);
                m_scrollToSelected = false;
            }
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemVisible())
            {
                const ImVec2 max(pos.x + width, pos.y + rowHeight);
                if (m_selected == static_cast<int>(index))
                    drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.14f)), Px(4.0f));
                else if (hovered)
                    drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.05f)), Px(4.0f));

                const float thumbWidth = pixel * static_cast<float>(UI::CreatureSprites::kColumns);
                const ImVec2 thumb(std::round(pos.x + Px(6.0f)), std::round(pos.y + Px(5.0f)));
                drawList->PushClipRect(pos, max, true);
                if (file.View)
                    Animation::DrawSprite(drawList, thumb, file.View->Get(), Animation::Mood::Idling, SeedOf(static_cast<int>(index)), pixel);
                else
                    drawList->AddText(ImVec2(thumb.x + thumbWidth * 0.5f - Px(6.0f), pos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f), Color(ManifoldTheme::Error),
                        ICON_FA_EXCLAMATION_TRIANGLE);

                const float textX = thumb.x + thumbWidth + Px(10.0f);
                const float line = ImGui::GetTextLineHeight();
                const float top = std::round(pos.y + (rowHeight - line * 2.0f) * 0.5f);
                std::string badge;
                ImVec4 badgeColor = ManifoldTheme::Muted;
                if (!file.View)
                {
                    badge = ICON_FA_TIMES_CIRCLE;
                    badgeColor = ManifoldTheme::Error;
                }
                else if (!file.Findings.Errors.empty())
                {
                    badge = std::string(ICON_FA_TIMES_CIRCLE " ") + std::to_string(file.Findings.Errors.size());
                    badgeColor = ManifoldTheme::Error;
                }
                else if (!file.Findings.Warnings.empty())
                {
                    badge = std::string(ICON_FA_EXCLAMATION_TRIANGLE " ") + std::to_string(file.Findings.Warnings.size());
                    badgeColor = ManifoldTheme::Warning;
                }
                const float badgeWidth = badge.empty() ? 0.0f : ImGui::CalcTextSize(badge.c_str()).x;
                const float right = max.x - Px(8.0f) - (badge.empty() ? 0.0f : badgeWidth + Px(8.0f));
                drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), max.y), true);
                drawList->AddText(ImVec2(textX, top), Color(ManifoldTheme::Text), file.Name.c_str());
                drawList->AddText(ImVec2(textX, top + line), Color(ManifoldTheme::Muted), file.Key.c_str());
                drawList->PopClipRect();
                if (!badge.empty())
                    drawList->AddText(ImVec2(max.x - Px(8.0f) - badgeWidth, std::round(pos.y + (rowHeight - line) * 0.5f)), Color(badgeColor), badge.c_str());
                drawList->PopClipRect();
            }
            ImGui::PopID();
        }
    }

    void Screen::DrawSceneRows()
    {
        const float rowHeight = std::round(Px(26.0f));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (const Backdrop backdrop : kBackdrops)
        {
            ImGui::PushID(static_cast<int>(backdrop));
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (ImGui::InvisibleButton("##scene", ImVec2(width, rowHeight)))
                m_backdrop = backdrop;
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 max(pos.x + width, pos.y + rowHeight);
            if (m_backdrop == backdrop)
                drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.14f)), Px(4.0f));
            else if (hovered)
                drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.05f)), Px(4.0f));
            const float textY = std::round(pos.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f);
            drawList->AddText(ImVec2(pos.x + Px(8.0f), textY), Color(m_backdrop == backdrop ? ManifoldTheme::Accent : ManifoldTheme::Label), BackdropIcon(backdrop));
            drawList->AddText(ImVec2(pos.x + Px(32.0f), textY), Color(ManifoldTheme::Text), BackdropName(backdrop));
            ImGui::PopID();
        }
    }

    void Screen::DrawLight()
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
        ImGui::TextUnformatted("Light");
        ImGui::PopStyleColor();
        const float gap = Px(4.0f);
        const float width = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;
        const float height = std::round(Px(24.0f));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (std::size_t index = 0; index < std::size(kLights); ++index)
        {
            if (index > 0)
                ImGui::SameLine(0.0f, gap);
            const Light& light = kLights[index];
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::InvisibleButton("##light", ImVec2(width, height)))
                m_daylight = light.Daylight;
            const bool hovered = ImGui::IsItemHovered();
            ImGui::PopID();
            const bool chosen = m_daylight == light.Daylight;
            const ImVec2 max(pos.x + width, pos.y + height);
            drawList->AddRectFilled(pos, max, Color(chosen ? ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.18f) : hovered ? ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.07f)
                : ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.03f)), Px(4.0f));
            const ImVec2 size = ImGui::CalcTextSize(light.Name);
            drawList->AddText(ImVec2(std::round(pos.x + (width - size.x) * 0.5f), std::round(pos.y + (height - size.y) * 0.5f)),
                Color(chosen ? ManifoldTheme::Accent : ManifoldTheme::Label), light.Name);
        }
    }

    void Screen::DrawExplorer(float width, float height)
    {
        BeginCard("##explorer", ICON_FA_FOLDER_OPEN, "Explorer", ImVec2(width, height));
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 resume = ImGui::GetCursorScreenPos();
        const float side = std::round(Px(22.0f));
        ImGui::SetCursorScreenPos(ImVec2(windowPos.x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - side, windowPos.y + Px(8.0f)));
        if (WindowButton("##reload", ICON_FA_SYNC, side, ManifoldTheme::Accent))
            Reload();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Read the creature files again");
        ImGui::SetCursorScreenPos(resume);

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", ICON_FA_SEARCH " Find by name or key", m_search.data(), m_search.size());
        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

        const float lightHeight = std::round(ImGui::GetTextLineHeightWithSpacing() + Px(24.0f) + Px(14.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##explorer_list", ImVec2(0.0f, -lightHeight), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleColor();

        const auto count = [&](bool evolved)
            {
                return static_cast<std::size_t>(std::count_if(m_files.begin(), m_files.end(), [&](const CreatureFile& file) { return file.Evolved == evolved; }));
            };
        SectionHeader("Creatures", count(false));
        DrawCreatureRows(false);
        SectionHeader("Evolved forms", count(true));
        DrawCreatureRows(true);
        SectionHeader("Scenes", 0);
        DrawSceneRows();
        ImGui::EndChild();

        ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));
        DrawLight();
        EndCard();
    }

    void Screen::DrawPaint(float width, float height)
    {
        const CreatureFile* file = Selected();
        std::string note;
        if (file && file->Sprite)
            note = "Frame 1 of " + std::to_string(file->Sprite->Frames.size());
        BeginCard("##paint", ICON_FA_PAINT_BRUSH, "Paint", ImVec2(width, height), note.c_str());
        if (file && !file->View)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
            ImGui::TextWrapped("%s cannot be read: %s", file->Key.c_str(), file->Problem.c_str());
            ImGui::PopStyleColor();
        }
        DrawGrid(file && file->View ? &file->View->Get() : nullptr, 0);
        EndCard();
    }

    void Screen::DrawPreview(float width, float height)
    {
        const CreatureFile* file = Selected();
        BeginCard("##preview", ICON_FA_EYE, "Preview", ImVec2(width, height), BackdropName(m_backdrop));
        const UI::CreatureSprites::Sprite* sprite = file && file->View ? &file->View->Get() : nullptr;
        const Habit habit = file && file->Sprite ? SpriteHabit(*file->Sprite) : Habit::Walks;
        const std::uint32_t seed = SeedOf(m_selected);

        const float tileHeight = std::round(Px(3.0f)) * static_cast<float>(UI::CreatureSprites::kRows) + Px(14.0f) + ImGui::GetTextLineHeight() + Px(8.0f);
        const float boxWidth = ImGui::GetContentRegionAvail().x;
        const float boxHeight = std::max(Px(90.0f), std::min(ImGui::GetContentRegionAvail().y - tileHeight * 2.0f - Px(12.0f), boxWidth * 0.62f));
        const ImVec2 min = ImGui::GetCursorScreenPos();
        DrawScenePreview(min, ImVec2(min.x + boxWidth, min.y + boxHeight), sprite, habit, m_backdrop, m_daylight, seed);
        ImGui::Dummy(ImVec2(boxWidth, boxHeight));
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));

        if (sprite)
        {
            const float gap = Px(6.0f);
            const float tileWidth = std::floor((boxWidth - gap * 3.0f) / 4.0f);
            const float small = Animation::PixelSize();
            const float collection = std::max(small, std::round(Px(3.0f)));
            int placed = 0;
            const auto next = [&]()
                {
                    if (placed % 4 != 0)
                        ImGui::SameLine(0.0f, gap);
                    ++placed;
                };
            next();
            SmallPreview("In a list", ManifoldTheme::Card, *sprite, small, Animation::Mood::Idling, Animation::Look::Normal, seed, tileWidth);
            next();
            SmallPreview("Idling", kRunningRow, *sprite, small, Animation::Mood::Idling, Animation::Look::Normal, seed, tileWidth);
            next();
            SmallPreview("Collection", ManifoldTheme::Sidebar, *sprite, collection, Animation::Mood::Idling, Animation::Look::Normal, seed, tileWidth);
            next();
            SmallPreview("Not found", ManifoldTheme::Sidebar, *sprite, collection, Animation::Mood::Idling, Animation::Look::Unknown, seed, tileWidth);
            next();
            SmallPreview("Asleep", ManifoldTheme::Sidebar, *sprite, collection, Animation::Mood::Sleeping, Animation::Look::Normal, seed, tileWidth);
            next();
            SmallPreview("Shiny", ManifoldTheme::Sidebar, *sprite, collection, Animation::Mood::Idling, Animation::Look::Shiny, seed, tileWidth);
            if (const CreatureFile* other = Counterpart(*file); other && other->View)
            {
                next();
                SmallPreview(other->Evolved ? "Evolved" : "First form", ManifoldTheme::Sidebar, other->View->Get(), collection, Animation::Mood::Idling, Animation::Look::Normal,
                    seed, tileWidth);
            }
        }
        EndCard();
    }

    void Screen::DrawProperties(float width, float height)
    {
        const CreatureFile* file = Selected();
        BeginCard("##properties", ICON_FA_SLIDERS_H, "Properties", ImVec2(width, height));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##properties_body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleColor();
        if (!file)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextWrapped("Choose a creature in the Explorer.");
            ImGui::PopStyleColor();
        }
        else
        {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            BoldText(ImGui::GetWindowDrawList(), at, ManifoldTheme::Text, file->Name.c_str());
            ImGui::Dummy(ImVec2(BoldWidth(file->Name.c_str()), ImGui::GetTextLineHeight()));
            FactRow("Key", file->Key, ManifoldTheme::Label);
            FactRow("File", WideToUtf8(file->Path.filename().wstring()), ManifoldTheme::Label);
            if (file->Sprite)
            {
                const SpriteFile::Sprite& sprite = *file->Sprite;
                char loop[32];
                std::snprintf(loop, sizeof(loop), "%.1f s", SpriteFile::LoopSeconds(sprite));
                FactRow("Frames", std::to_string(sprite.Frames.size()));
                FactRow("Steps", std::to_string(sprite.Steps.size()) + ", " + loop);
                FactRow("Moves", HabitName(SpriteHabit(sprite)));
                FactRow("Faces", sprite.Facing < 0 ? "left" : sprite.Facing > 0 ? "right" : "ahead");
                ColorRow("Body", sprite.Body);
                ColorRow("Second", sprite.Second);
                ColorRow("Third", sprite.Third);
            }

            ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted("Checks");
            ImGui::PopStyleColor();
            if (!file->Sprite)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Error);
                ImGui::TextWrapped(ICON_FA_TIMES_CIRCLE " Cannot be read: %s", file->Problem.c_str());
                ImGui::PopStyleColor();
            }
            else if (file->Findings.Errors.empty() && file->Findings.Warnings.empty())
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
                ImGui::TextUnformatted(ICON_FA_CHECK_CIRCLE " Nothing to fix");
                ImGui::PopStyleColor();
            }
            else
            {
                for (const std::string& error : file->Findings.Errors)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Error);
                    ImGui::TextWrapped(ICON_FA_TIMES_CIRCLE " %s", error.c_str());
                    ImGui::PopStyleColor();
                }
                for (const std::string& warning : file->Findings.Warnings)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
                    ImGui::TextWrapped(ICON_FA_EXCLAMATION_TRIANGLE " %s", warning.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndChild();
        EndCard();
    }

    void Screen::Draw(WindowControls& window)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::Begin("##creator", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(3);

        Animation::BeginFrame();
        TitleBar(window, m_folder, m_folderExists, m_files.size());

        const float pad = std::round(Px(14.0f));
        const float gap = std::round(Px(12.0f));
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 top = ImGui::GetCursorScreenPos();
        const ImVec2 start(windowPos.x + pad, top.y + pad);
        const float width = ImGui::GetWindowWidth() - pad * 2.0f;
        const float height = windowPos.y + ImGui::GetWindowHeight() - pad - start.y;

        const float explorer = std::round(Px(270.0f));
        const float side = std::round(Px(330.0f));
        const float timeline = std::round(Px(130.0f));
        const float upper = std::max(Px(200.0f), height - timeline - gap);
        const float paint = std::max(Px(160.0f), width - explorer - side - gap * 2.0f);
        const float previewHeight = std::round(upper * 0.6f);
        const float middleX = start.x + explorer + gap;
        const float sideX = middleX + paint + gap;

        ImGui::SetCursorScreenPos(start);
        DrawExplorer(explorer, height);

        ImGui::SetCursorScreenPos(ImVec2(middleX, start.y));
        DrawPaint(paint, upper);

        ImGui::SetCursorScreenPos(ImVec2(sideX, start.y));
        DrawPreview(side, previewHeight);

        ImGui::SetCursorScreenPos(ImVec2(sideX, start.y + previewHeight + gap));
        DrawProperties(side, upper - previewHeight - gap);

        ImGui::SetCursorScreenPos(ImVec2(middleX, start.y + upper + gap));
        BeginCard("##timeline", ICON_FA_FILM, "Timeline", ImVec2(paint + gap + side, timeline));
        EndCard();

        ImGui::End();
    }
}
