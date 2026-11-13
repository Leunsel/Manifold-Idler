#include "SpriteCreator/CreatorScreen.h"

#include "Core/Errors.h"
#include "Core/SpriteFile.h"
#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"
#include "SpriteCreator/Widgets.h"
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
        constexpr double kMessageSeconds = 6.0;

        struct Light
        {
            const char* Name;
            float Daylight;
        };
        constexpr Light kLights[] = { { "Day", 1.0f }, { "Dawn", 0.55f }, { "Dusk", 0.45f }, { "Night", 0.0f } };

        std::uint32_t SeedOf(int index)
        {
            return static_cast<std::uint32_t>(index) * 7919u + 13u;
        }

        std::string_view StemOf(const CreatureFile& file)
        {
            constexpr std::string_view kPrefix = "evolved/";
            std::string_view key = file.Key;
            if (file.Evolved && key.substr(0, kPrefix.size()) == kPrefix)
                key.remove_prefix(kPrefix.size());
            return key;
        }

        struct TitleStatus
        {
            std::string Text;
            ImVec4 Color;
        };

        void TitleBar(WindowControls& window, const TitleStatus& status)
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

            const float statusLeft = x + Px(18.0f);
            const float statusRight = buttonsX - Px(16.0f);
            if (statusRight > statusLeft)
            {
                drawList->PushClipRect(ImVec2(statusLeft, origin.y), ImVec2(statusRight, origin.y + height), true);
                drawList->AddText(ImVec2(statusLeft, textY), Color(status.Color), status.Text.c_str());
                drawList->PopClipRect();
            }

            const float buttonsY = std::round(origin.y + (height - side) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(buttonsX, buttonsY));
            if (IconButton("##minimise", ICON_FA_MINUS, side, ManifoldTheme::Text))
                window.Minimise();
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##maximise", window.Maximised() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, side, ManifoldTheme::Text))
                window.ToggleMaximised();
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##close", ICON_FA_TIMES, side, ManifoldTheme::Error))
                window.Close();

            window.AddDragArea(origin.x, origin.y, buttonsX, origin.y + height);
            window.AddDragArea(buttonsX, origin.y, origin.x + width, buttonsY);
            ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
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

        bool DialogButton(const char* label, bool primary)
        {
            if (primary)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.22f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.32f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.42f));
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Accent);
            }
            const bool clicked = ImGui::Button(label);
            if (primary)
                ImGui::PopStyleColor(4);
            return clicked;
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
        m_paint.Stroking = false;
        ClampFrame(Selected(), m_paint);
    }

    void Screen::Select(std::string_view key)
    {
        for (std::size_t index = 0; index < m_files.size(); ++index)
        {
            if (m_files[index].Key == key)
            {
                ChangeSelection(static_cast<int>(index));
                m_scrollToSelected = true;
            }
        }
    }

    void Screen::ChangeSelection(int index)
    {
        if (index == m_selected || m_paint.Stroking)
            return;
        m_selected = index;
        m_paint.Frame = 0;
        m_timeline.Step = -1;
        m_timeline.SeenFrame = -1;
        m_timeline.Position = 0.0;
        m_timeline.Settle = false;
        m_timeline.Started = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    }

    CreatureFile* Screen::Selected()
    {
        if (m_selected < 0 || m_selected >= static_cast<int>(m_files.size()))
            return nullptr;
        return &m_files[static_cast<std::size_t>(m_selected)];
    }

    const CreatureFile* Screen::Selected() const
    {
        if (m_selected < 0 || m_selected >= static_cast<int>(m_files.size()))
            return nullptr;
        return &m_files[static_cast<std::size_t>(m_selected)];
    }

    const CreatureFile* Screen::Counterpart(const CreatureFile& file) const
    {
        const std::string_view stem = StemOf(file);
        for (const CreatureFile& other : m_files)
        {
            if (&other != &file && other.Evolved != file.Evolved && StemOf(other) == stem)
                return &other;
        }
        return nullptr;
    }

    CreatureFile* Screen::Counterpart(const CreatureFile& file)
    {
        return const_cast<CreatureFile*>(std::as_const(*this).Counterpart(file));
    }

    const CreatureFile* Screen::FirstFormFor(const CreatureFile* file) const
    {
        if (!file || file->Evolved || !file->Sprite || Counterpart(*file))
            return nullptr;
        std::error_code ignored;
        if (std::filesystem::exists(m_folder / "evolved" / (std::string(StemOf(*file)) + ".json"), ignored))
            return nullptr;
        return file;
    }

    void Screen::Jump(const PropertiesRequests& requests)
    {
        CreatureFile* file = Selected();
        if (!file || !file->Sprite || m_paint.Stroking)
            return;
        if (m_timeline.Playing)
            TogglePlaying(m_timeline);
        m_timeline.Settle = false;
        const std::vector<SpriteFile::Step>& steps = file->Sprite->Steps;
        int frame = requests.JumpFrame;
        if (requests.JumpStep >= 0 && requests.JumpStep < static_cast<int>(steps.size()))
        {
            m_timeline.Step = requests.JumpStep;
            if (frame < 0)
                frame = steps[static_cast<std::size_t>(requests.JumpStep)].Frame;
        }
        if (frame >= 0)
        {
            m_paint.Frame = frame;
            ClampFrame(file, m_paint);
        }
    }

    std::size_t Screen::UnsavedCount() const
    {
        return static_cast<std::size_t>(std::count_if(m_files.begin(), m_files.end(), [](const CreatureFile& file) { return file.Dirty(); }));
    }

    void Screen::Say(std::string text, bool problem)
    {
        m_message = std::move(text);
        m_messageIsProblem = problem;
        m_messageAt = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    }

    void Screen::UndoSelected(bool forward)
    {
        CreatureFile* file = Selected();
        if (!file || !file->Sprite || m_paint.Stroking)
            return;
        const Motion before = MotionOf(*file->Sprite);
        if (!(forward ? StepForward(*file) : StepBack(*file)) || file->Evolved || !file->Sprite)
            return;
        const Motion after = MotionOf(*file->Sprite);
        CreatureFile* evolved = Counterpart(*file);
        if (after == before || !evolved || !evolved->Sprite)
            return;
        Change(*evolved, [&](SpriteFile::Sprite& edited) { return SetMotion(edited, after); });
    }

    void Screen::SaveSelected()
    {
        CreatureFile* file = Selected();
        if (!file || !file->Dirty() || m_paint.Stroking)
            return;
        std::string problem;
        if (SaveCreature(*file, problem))
            Say("Saved " + WideToUtf8(file->Path.filename().wstring()) + ".", false);
        else
            Say(ErrorMessage(ErrorCode::SpriteNotSaved, problem), true);
    }

    bool Screen::SaveAll()
    {
        std::size_t saved = 0;
        for (CreatureFile& file : m_files)
        {
            if (!file.Dirty())
                continue;
            std::string problem;
            if (!SaveCreature(file, problem))
            {
                Say(ErrorMessage(ErrorCode::SpriteNotSaved, problem), true);
                return false;
            }
            ++saved;
        }
        Say(saved == 1 ? std::string("Saved 1 creature.") : "Saved " + std::to_string(saved) + " creatures.", false);
        return true;
    }

    void Screen::RequestClose()
    {
        if (UnsavedCount() > 0)
            m_pending = Pending::Close;
        else
            m_quit = true;
    }

    bool Screen::TakeQuit()
    {
        const bool quit = m_quit;
        m_quit = false;
        return quit;
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

    void Screen::HandleKeys()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput || m_pending != Pending::None || m_new.Open || m_paint.Stroking)
            return;
        CreatureFile* file = Selected();
        if (io.KeyCtrl)
        {
            if (file && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))
                UndoSelected(true);
            else if (file && ImGui::IsKeyPressed(ImGuiKey_Z))
                UndoSelected(false);
            else if (ImGui::IsKeyPressed(ImGuiKey_S, false))
                SaveSelected();
            else if (ImGui::IsKeyPressed(ImGuiKey_N, false))
                OpenNewCreature();
            return;
        }
        if (io.KeyAlt)
            return;
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            TogglePlaying(m_timeline);
        const struct
        {
            ImGuiKey Key;
            Tool Kind;
        } tools[] = { { ImGuiKey_B, Tool::Pencil }, { ImGuiKey_E, Tool::Eraser }, { ImGuiKey_G, Tool::Fill }, { ImGuiKey_L, Tool::Line },
            { ImGuiKey_R, Tool::Rectangle }, { ImGuiKey_I, Tool::Picker }, { ImGuiKey_M, Tool::Move } };
        for (const auto& tool : tools)
        {
            if (ImGui::IsKeyPressed(tool.Key, false))
                m_paint.Current = tool.Kind;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_X, false))
            m_paint.Mirror = !m_paint.Mirror;
        if (ImGui::IsKeyPressed(ImGuiKey_O, false))
            m_paint.Onion = !m_paint.Onion;
        if (file && ImGui::IsKeyPressed(ImGuiKey_H, false))
            FlipFrame(*file, m_paint);
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
            StepFrame(file, m_paint, -1);
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
            StepFrame(file, m_paint, 1);
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
                ChangeSelection(static_cast<int>(index));
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
                float right = max.x - Px(8.0f) - (badge.empty() ? 0.0f : badgeWidth + Px(8.0f));
                if (file.Dirty())
                {
                    const float radius = Px(3.5f);
                    drawList->AddCircleFilled(ImVec2(right - radius, std::round(pos.y + rowHeight * 0.5f)), radius, Color(ManifoldTheme::Warning));
                    right -= radius * 2.0f + Px(8.0f);
                }
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
        const float right = windowPos.x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
        ImGui::SetCursorScreenPos(ImVec2(right - side * 2.0f - Px(4.0f), windowPos.y + Px(8.0f)));
        if (IconButton("##new", ICON_FA_PLUS, side, ManifoldTheme::Accent, false, m_folderExists))
            OpenNewCreature();
        Tooltip("New creature (Ctrl+N)");
        ImGui::SetCursorScreenPos(ImVec2(right - side, windowPos.y + Px(8.0f)));
        if (IconButton("##reload", ICON_FA_SYNC, side, ManifoldTheme::Accent))
        {
            if (UnsavedCount() > 0)
                m_pending = Pending::Reload;
            else
                Reload();
        }
        Tooltip("Read the creature files again");
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

    void Screen::DrawPreview(float width, float height)
    {
        const CreatureFile* file = Selected();
        BeginCard("##preview", ICON_FA_EYE, "Preview", ImVec2(width, height), BackdropName(m_backdrop));
        const UI::CreatureSprites::Sprite* sprite = file && file->View ? &file->View->Get() : nullptr;
        const Habit habit = file && file->Sprite ? SpriteHabit(*file->Sprite) : Habit::Walks;
        const std::uint32_t seed = SeedOf(m_selected);

        const float tileHeight = std::round(Px(3.0f)) * static_cast<float>(UI::CreatureSprites::kRows) + Px(14.0f) + ImGui::GetTextLineHeight() + Px(8.0f);
        const float boxWidth = ImGui::GetContentRegionAvail().x;
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float reserve = tileHeight * 2.0f + spacing * 4.0f + Px(4.0f);
        const float boxHeight = std::max(Px(90.0f), std::min(ImGui::GetContentRegionAvail().y - reserve, boxWidth * 0.62f));
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

    void Screen::DrawUnsavedQuestion()
    {
        if (m_pending == Pending::None)
            return;
        if (!ImGui::IsPopupOpen("##unsaved"))
            ImGui::OpenPopup("##unsaved");

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::round(Px(460.0f)), 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(18.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(8.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ManifoldTheme::Card);
        const bool open = ImGui::BeginPopupModal("##unsaved", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        if (!open)
            return;

        const bool closing = m_pending == Pending::Close;
        const char* title = closing ? "Close the Sprite Creator?" : "Read the creature files again?";
        BoldText(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ManifoldTheme::Text, title);
        ImGui::Dummy(ImVec2(BoldWidth(title), ImGui::GetTextLineHeight()));
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));

        const std::size_t unsaved = UnsavedCount();
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Label);
        ImGui::TextWrapped("%s", unsaved == 1 ? "This creature has changes that are not saved:" : (std::to_string(unsaved) + " creatures have changes that are not saved:").c_str());
        ImGui::PopStyleColor();
        std::size_t listed = 0;
        for (const CreatureFile& file : m_files)
        {
            if (!file.Dirty())
                continue;
            if (listed == 6)
            {
                MutedText(("and " + std::to_string(unsaved - listed) + " more").c_str());
                break;
            }
            ImGui::BulletText("%s", file.Name.c_str());
            ++listed;
        }
        ImGui::Dummy(ImVec2(0.0f, Px(8.0f)));

        Pending done = Pending::None;
        bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        if (DialogButton(closing ? "Save all and close" : "Save all and read again", true))
        {
            if (SaveAll())
                done = m_pending;
            else
                cancel = true;
        }
        ImGui::SameLine();
        if (DialogButton(closing ? "Close without saving" : "Read again without them", false))
            done = m_pending;
        ImGui::SameLine();
        if (DialogButton("Cancel", false))
            cancel = true;

        if (done != Pending::None || cancel)
        {
            m_pending = Pending::None;
            ImGui::CloseCurrentPopup();
            if (done == Pending::Close)
                m_quit = true;
            else if (done == Pending::Reload)
                Reload();
        }
        ImGui::EndPopup();
    }

    void Screen::OpenNewCreature()
    {
        if (!m_folderExists || m_paint.Stroking || m_pending != Pending::None)
            return;
        m_new = {};
        m_new.Open = true;
        m_new.Focus = true;
    }

    void Screen::CreateCreature(const std::string& key, const std::string& id, const CreatureFile* source, const CreatureFile* firstForm)
    {
        SpriteFile::Sprite sprite = source && source->Sprite ? *source->Sprite : StarterSprite();
        if (firstForm && firstForm->Sprite)
        {
            const SpriteFile::Sprite& first = *firstForm->Sprite;
            sprite.Facing = first.Facing;
            sprite.Floats = first.Floats;
            sprite.Swims = first.Swims;
            sprite.Stays = first.Stays;
            if (!source)
            {
                sprite.Body = first.Body;
                sprite.Second = first.Second;
                sprite.Third = first.Third;
            }
        }
        CreatureFile created = NewCreature(m_folder, key, id, std::move(sprite), firstForm != nullptr);
        const std::string createdKey = created.Key;
        m_files.push_back(std::move(created));
        SortCreatures(m_files);
        m_selected = -1;
        m_search.fill('\0');
        Select(createdKey);
        Say("Made " + createdKey + ". It is written when you save (Ctrl+S).", false);
    }

    void Screen::DrawNewCreature()
    {
        if (!m_new.Open)
            return;
        if (!ImGui::IsPopupOpen("##new_creature"))
            ImGui::OpenPopup("##new_creature");

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(std::round(Px(500.0f)), 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, PxWhole(18.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(8.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ManifoldTheme::Card);
        const bool open = ImGui::BeginPopupModal("##new_creature", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        if (!open)
            return;

        constexpr const char* kTitle = "New creature";
        BoldText(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), ManifoldTheme::Text, kTitle);
        ImGui::Dummy(ImVec2(BoldWidth(kTitle), ImGui::GetTextLineHeight()));
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));

        const float labelWidth = std::round(Px(96.0f));
        const CreatureFile* selected = Selected();
        const CreatureFile* firstForm = FirstFormFor(selected);
        if (!firstForm)
            m_new.Evolved = false;
        const bool canCopy = selected && selected->Sprite;
        if (!canCopy)
            m_new.Copy = false;

        FieldLabel("Make", labelWidth);
        if (Segment("##kind_new", "A new creature", !m_new.Evolved))
        {
            m_new.Evolved = false;
            m_new.Focus = true;
        }
        ImGui::SameLine(0.0f, Px(4.0f));
        const std::string evolvedLabel = firstForm ? "The evolved form of " + firstForm->Name : std::string("An evolved form");
        if (Segment("##kind_evolved", evolvedLabel.c_str(), m_new.Evolved, firstForm != nullptr))
            m_new.Evolved = true;
        if (!firstForm)
            Tooltip("Choose a creature without an evolved form in the Explorer first");

        const auto keyFilter = [](ImGuiInputTextCallbackData* data) -> int
            {
                const ImWchar c = data->EventChar;
                if (c >= 'A' && c <= 'Z')
                {
                    data->EventChar = static_cast<ImWchar>(c - 'A' + 'a');
                    return 0;
                }
                return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ? 0 : 1;
            };
        const auto idFilter = [](ImGuiInputTextCallbackData* data) -> int
            {
                const ImWchar c = data->EventChar;
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ? 0 : 1;
            };

        std::string key;
        std::string id;
        if (m_new.Evolved && firstForm)
        {
            key = std::string(StemOf(*firstForm));
            id = firstForm->Sprite->Id;
            FieldLabel("Key", labelWidth);
            ImGui::TextUnformatted(("evolved/" + key).c_str());
            FieldLabel("Id", labelWidth);
            ImGui::TextUnformatted(id.c_str());
        }
        else
        {
            FieldLabel("Key", labelWidth);
            ImGui::SetNextItemWidth(std::round(Px(200.0f)));
            if (m_new.Focus)
            {
                ImGui::SetKeyboardFocusHere();
                m_new.Focus = false;
            }
            if (ImGui::InputText("##key", m_new.Key.data(), m_new.Key.size(), ImGuiInputTextFlags_CallbackCharFilter, keyFilter) && !m_new.IdEdited)
            {
                const std::string suggested = IdFromKey(m_new.Key.data()).substr(0, m_new.Id.size() - 1);
                m_new.Id.fill('\0');
                std::copy(suggested.begin(), suggested.end(), m_new.Id.begin());
            }
            ImGui::SameLine(0.0f, Px(10.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted("its file name");
            ImGui::PopStyleColor();

            FieldLabel("Id", labelWidth);
            ImGui::SetNextItemWidth(std::round(Px(200.0f)));
            if (ImGui::InputText("##id", m_new.Id.data(), m_new.Id.size(), ImGuiInputTextFlags_CallbackCharFilter, idFilter))
                m_new.IdEdited = std::string_view(m_new.Id.data()) != IdFromKey(m_new.Key.data());
            ImGui::SameLine(0.0f, Px(10.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted("its name in the program code");
            ImGui::PopStyleColor();
            key = m_new.Key.data();
            id = m_new.Id.data();
        }

        FieldLabel("Start from", labelWidth);
        if (Segment("##start_shape", "A simple shape", !m_new.Copy))
            m_new.Copy = false;
        ImGui::SameLine(0.0f, Px(4.0f));
        const std::string copyLabel = canCopy ? "A copy of " + selected->Name : std::string("A copy");
        if (Segment("##start_copy", copyLabel.c_str(), m_new.Copy, canCopy))
            m_new.Copy = true;
        if (!canCopy)
            Tooltip("Choose the creature to copy in the Explorer first");

        std::string problem;
        if (!m_new.Evolved)
        {
            std::error_code ignored;
            const bool keyTaken = std::any_of(m_files.begin(), m_files.end(), [&](const CreatureFile& file) { return !file.Evolved && StemOf(file) == key; })
                || (!key.empty() && std::filesystem::exists(m_folder / (key + ".json"), ignored));
            const bool idTaken = std::any_of(m_files.begin(), m_files.end(), [&](const CreatureFile& file) { return !file.Evolved && file.Sprite && file.Sprite->Id == id; });
            if (key.empty())
                problem = "Give it a key: small letters and digits, starting with a letter.";
            else if (!IsKeyText(key))
                problem = "A key starts with a letter and has at most 32 small letters and digits.";
            else if (keyTaken)
                problem = "There is a creature with this key already.";
            else if (!IsIdText(id))
                problem = "An id starts with a capital letter and has at most 32 letters and digits.";
            else if (idTaken)
                problem = "Another creature has this id already.";
        }
        ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
        if (!problem.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, key.empty() ? ManifoldTheme::Muted : ManifoldTheme::Warning);
            ImGui::TextWrapped("%s", problem.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            const std::string where = m_new.Evolved ? "creatures/evolved/" + key + ".json" : "creatures/" + key + ".json";
            MutedText(("Saving writes " + where + ".").c_str());
        }
        ImGui::Dummy(ImVec2(0.0f, Px(8.0f)));

        bool create = false;
        bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
        ImGui::BeginDisabled(!problem.empty());
        if (DialogButton("Make it", true))
            create = true;
        ImGui::EndDisabled();
        if (problem.empty() && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            create = true;
        ImGui::SameLine();
        if (DialogButton("Cancel", false))
            cancel = true;

        if (create || cancel)
        {
            m_new.Open = false;
            ImGui::CloseCurrentPopup();
            if (create)
                CreateCreature(key, id, m_new.Copy ? selected : nullptr, m_new.Evolved ? firstForm : nullptr);
        }
        ImGui::EndPopup();
    }

    void Screen::Draw(WindowControls& window)
    {
        HandleKeys();

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

        TitleStatus status{ std::string(), ManifoldTheme::Muted };
        if (!m_message.empty() && ImGui::GetTime() - m_messageAt < kMessageSeconds)
        {
            status = { m_message, m_messageIsProblem ? ManifoldTheme::Error : ManifoldTheme::Accent };
            Animation::RequestFrames();
        }
        else if (!m_folderExists)
        {
            status = { m_folder.empty() ? ErrorMessage(ErrorCode::NoCreatureFolder) : ErrorMessage(ErrorCode::CreatureFolderMissing, WideToUtf8(m_folder.wstring())),
                ManifoldTheme::Warning };
        }
        else
        {
            status.Text = WideToUtf8(m_folder.wstring()) + "  \xC2\xB7  " + std::to_string(m_files.size()) + (m_files.size() == 1 ? " file" : " files");
            if (const std::size_t unsaved = UnsavedCount(); unsaved > 0)
            {
                status.Text = std::to_string(unsaved) + " not saved  \xC2\xB7  " + status.Text;
                status.Color = ManifoldTheme::Warning;
            }
        }
        TitleBar(window, status);

        const float pad = std::round(Px(14.0f));
        const float gap = std::round(Px(12.0f));
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 top = ImGui::GetCursorScreenPos();
        const ImVec2 start(windowPos.x + pad, top.y + pad);
        const float width = ImGui::GetWindowWidth() - pad * 2.0f;
        const float height = windowPos.y + ImGui::GetWindowHeight() - pad - start.y;

        const float explorer = std::round(Px(240.0f));
        const float side = std::round(Px(330.0f));
        const float timeline = std::max(std::round(Px(200.0f)), TimelineCardHeight());
        const float upper = std::max(Px(200.0f), height - timeline - gap);
        const float middle = std::max(Px(160.0f), width - explorer - side - gap * 2.0f);
        const ImGuiStyle& style = ImGui::GetStyle();
        const float tileHeight = std::round(Px(3.0f)) * static_cast<float>(UI::CreatureSprites::kRows) + Px(14.0f) + ImGui::GetTextLineHeight() + Px(8.0f);
        const float previewBox = (side - Px(28.0f)) * 0.62f;
        const float previewNeeds = Px(24.0f) + ImGui::GetTextLineHeight() + Px(4.0f) + style.ItemSpacing.y * 6.0f + previewBox + tileHeight * 2.0f + Px(4.0f);
        const float previewHeight = std::round(std::min(height * 0.6f, previewNeeds));
        const float middleX = start.x + explorer + gap;
        const float sideX = middleX + middle + gap;

        ImGui::SetCursorScreenPos(start);
        DrawExplorer(explorer, height);

        ImGui::SetCursorScreenPos(ImVec2(middleX, start.y));
        const PaintRequests requests = DrawPaintCard(Selected(), m_paint, ImVec2(middle, upper));
        if (CreatureFile* file = Selected(); file && !m_paint.Stroking)
        {
            if (requests.Undo)
                UndoSelected(false);
            if (requests.Redo)
                UndoSelected(true);
        }
        if (requests.Save)
            SaveSelected();

        ImGui::SetCursorScreenPos(ImVec2(middleX, start.y + upper + gap));
        DrawTimelineCard(Selected(), m_paint, m_timeline, ImVec2(middle, timeline));

        ImGui::SetCursorScreenPos(ImVec2(sideX, start.y));
        DrawPreview(side, previewHeight);

        ImGui::SetCursorScreenPos(ImVec2(sideX, start.y + previewHeight + gap));
        CreatureFile* file = Selected();
        const PropertiesRequests asked = DrawPropertiesCard(file, file ? Counterpart(*file) : nullptr, m_properties, ImVec2(side, height - previewHeight - gap));
        if (asked.JumpFrame >= 0 || asked.JumpStep >= 0)
            Jump(asked);

        DrawUnsavedQuestion();
        DrawNewCreature();
        ImGui::End();
    }
}
