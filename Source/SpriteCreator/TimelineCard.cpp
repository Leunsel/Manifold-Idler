#include "SpriteCreator/TimelineCard.h"

#include "Core/SpriteEdit.h"
#include "Generated/IconsFontAwesome5.h"
#include "SpriteCreator/Widgets.h"
#include "UI/IdleAnimation.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;
        namespace Animation = UI::IdleAnimation;

        constexpr ImVec4 kFrameTints[] = {
            ImVec4(0.18f, 0.86f, 0.62f, 1.0f), ImVec4(0.36f, 0.58f, 0.95f, 1.0f), ImVec4(0.70f, 0.48f, 0.95f, 1.0f), ImVec4(0.95f, 0.45f, 0.65f, 1.0f),
            ImVec4(0.96f, 0.62f, 0.30f, 1.0f), ImVec4(0.92f, 0.82f, 0.32f, 1.0f), ImVec4(0.55f, 0.85f, 0.35f, 1.0f), ImVec4(0.35f, 0.80f, 0.90f, 1.0f),
        };

        ImVec4 TintOf(int frame)
        {
            return kFrameTints[static_cast<std::size_t>(std::max(0, frame)) % std::size(kFrameTints)];
        }

        double PlayedTicks(const TimelineState& timeline)
        {
            return (ImGui::GetTime() - timeline.Started) / SpriteFile::kTickSeconds;
        }

        void Label(const char* text, float width)
        {
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddText(ImVec2(at.x, at.y), Color(ManifoldTheme::Muted), text);
            ImGui::Dummy(ImVec2(width, ImGui::GetTextLineHeight()));
        }

        void Header(const CreatureFile& file, TimelineState& timeline)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            char loop[64];
            std::snprintf(loop, sizeof(loop), "Loop %.1f s, %zu %s", SpriteFile::LoopSeconds(sprite), sprite.Steps.size(), sprite.Steps.size() == 1 ? "step" : "steps");
            const float side = std::round(Px(22.0f));
            const float textWidth = ImGui::CalcTextSize(loop).x;
            const ImVec2 windowPos = ImGui::GetWindowPos();
            const ImVec2 resume = ImGui::GetCursorScreenPos();
            const float right = windowPos.x + ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x;
            const float top = windowPos.y + Px(8.0f);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddText(ImVec2(right - side - Px(8.0f) - textWidth, std::round(top + (side - ImGui::GetTextLineHeight()) * 0.5f)), Color(ManifoldTheme::Muted), loop);
            ImGui::SetCursorScreenPos(ImVec2(right - side, top));
            if (IconButton("##play", timeline.Playing ? ICON_FA_PAUSE : ICON_FA_PLAY, side, ManifoldTheme::Accent, timeline.Playing))
                TogglePlaying(timeline);
            Tooltip(timeline.Playing ? "Stop playing (Space)" : "Play the loop (Space)");
            ImGui::SetCursorScreenPos(resume);
        }

        void FrameButtons(CreatureFile& file, PaintState& paint, const TimelineState& timeline, float width)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            const int count = static_cast<int>(sprite.Frames.size());
            const float side = std::round(Px(24.0f));
            const float buttonGap = Px(2.0f);
            const float buttonsWidth = side * 6.0f + buttonGap * 5.0f;
            const ImVec2 rowStart = ImGui::GetCursorScreenPos();
            constexpr const char* kLabel = "Frames";
            ImGui::GetWindowDrawList()->AddText(ImVec2(rowStart.x, std::round(rowStart.y + (side - ImGui::GetTextLineHeight()) * 0.5f)), Color(ManifoldTheme::Muted), kLabel);
            const float buttonsX = std::max(rowStart.x + ImGui::CalcTextSize(kLabel).x + Px(10.0f), rowStart.x + width - buttonsWidth);
            ImGui::SetCursorScreenPos(ImVec2(buttonsX, rowStart.y));
            const int at = paint.Frame;
            const bool free = count < SpriteFile::kMaxFrames && !timeline.Playing;
            if (IconButton("##add_frame", ICON_FA_PLUS, side, ManifoldTheme::Text, false, free))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::InsertFrame(edited, at + 1, SpriteEdit::EmptyFrame()); }))
                    paint.Frame = at + 1;
            }
            Tooltip("Add an empty frame after this one");
            ImGui::SameLine(0.0f, buttonGap);
            if (IconButton("##copy_frame", ICON_FA_CLONE, side, ManifoldTheme::Text, false, free))
            {
                const SpriteFile::Frame copy = sprite.Frames[static_cast<std::size_t>(at)];
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::InsertFrame(edited, at + 1, copy); }))
                    paint.Frame = at + 1;
            }
            Tooltip("Copy this frame");
            ImGui::SameLine(0.0f, buttonGap);
            if (IconButton("##frame_left", ICON_FA_ARROW_LEFT, side, ManifoldTheme::Text, false, at > 0 && !timeline.Playing))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::MoveFrame(edited, at, at - 1); }))
                    paint.Frame = at - 1;
            }
            Tooltip("Move this frame to the left; the steps follow it");
            ImGui::SameLine(0.0f, buttonGap);
            if (IconButton("##frame_right", ICON_FA_ARROW_RIGHT, side, ManifoldTheme::Text, false, at + 1 < count && !timeline.Playing))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::MoveFrame(edited, at, at + 1); }))
                    paint.Frame = at + 1;
            }
            Tooltip("Move this frame to the right; the steps follow it");
            ImGui::SameLine(0.0f, buttonGap);
            if (IconButton("##sleep_frame", ICON_FA_MOON, side, ManifoldTheme::Text, sprite.Sleep == at, sprite.Sleep != at))
            {
                Change(file, [&](SpriteFile::Sprite& edited)
                    {
                        edited.Sleep = at;
                        return true;
                    });
            }
            Tooltip("Sleep in this frame");
            ImGui::SameLine(0.0f, buttonGap);
            if (IconButton("##remove_frame", ICON_FA_TRASH_ALT, side, ManifoldTheme::Error, false, count > 1 && !timeline.Playing))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::RemoveFrame(edited, at); }))
                    paint.Frame = std::max(0, at - 1);
            }
            Tooltip("Remove this frame and the steps that show it");

            ImGui::SetCursorScreenPos(rowStart);
            ImGui::Dummy(ImVec2(width, side));
        }

        ImVec2 FrameBox()
        {
            const float pixel = std::max(2.0f, std::round(Px(3.0f)));
            const float inset = std::round(Px(3.0f));
            return ImVec2(pixel * static_cast<float>(SpriteFile::kColumns) + inset * 2.0f, pixel * static_cast<float>(SpriteFile::kRows) + inset * 2.0f);
        }

        void FrameRow(CreatureFile& file, PaintState& paint, TimelineState& timeline, float width)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            const int count = static_cast<int>(sprite.Frames.size());
            std::set<int> used;
            for (const SpriteFile::Step& step : sprite.Steps)
                used.insert(step.Frame);

            const float pixel = std::max(2.0f, std::round(Px(3.0f)));
            const float inset = std::round(Px(3.0f));
            const ImVec2 box = FrameBox();
            const float gap = Px(6.0f);
            const float rowWidth = box.x * static_cast<float>(count) + gap * static_cast<float>(std::max(0, count - 1));

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##frames", ImVec2(width, box.y + ImGui::GetStyle().ScrollbarSize), ImGuiChildFlags_None,
                ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleColor();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int index = 0; index < count; ++index)
            {
                if (index > 0)
                    ImGui::SameLine(0.0f, gap);
                ImGui::PushID(index);
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##frame", box) && !timeline.Playing)
                    paint.Frame = index;
                const bool hovered = ImGui::IsItemHovered();
                const ImVec2 max(pos.x + box.x, pos.y + box.y);
                const bool current = paint.Frame == index;
                if (current && timeline.SeenFrame != index)
                {
                    const float left = ImGui::GetWindowPos().x;
                    if (rowWidth > width && (pos.x < left || max.x > left + ImGui::GetWindowWidth()))
                        ImGui::SetScrollHereX(0.5f);
                    timeline.SeenFrame = index;
                }
                const bool unused = !used.contains(index) && sprite.Sleep != index;
                drawList->AddRectFilled(pos, max, Color(ManifoldTheme::Sidebar), Px(4.0f));
                if (file.View)
                    Animation::DrawSpriteFrame(drawList, ImVec2(pos.x + inset, pos.y + inset), file.View->Get(), index, pixel, false, Animation::Tint::Colors);
                if (unused)
                    drawList->AddRectFilled(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Sidebar, 0.55f)), Px(4.0f));
                const ImVec4 border = current ? ManifoldTheme::Accent : hovered ? ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.35f) : ManifoldTheme::WithAlpha(TintOf(index), 0.55f);
                drawList->AddRect(pos, max, Color(border), Px(4.0f), 0, current ? Px(2.0f) : Px(1.0f));
                const std::string number = std::to_string(index + 1);
                drawList->AddText(ImVec2(pos.x + Px(4.0f), pos.y + Px(2.0f)), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.7f)), number.c_str());
                if (sprite.Sleep == index)
                {
                    const ImVec2 moonSize = ImGui::CalcTextSize(ICON_FA_MOON);
                    drawList->AddText(ImVec2(max.x - moonSize.x - Px(4.0f), pos.y + Px(2.0f)), Color(ManifoldTheme::Label), ICON_FA_MOON);
                }
                std::string tip = "Frame " + number;
                if (sprite.Sleep == index)
                    tip += ", the sleeping frame";
                if (unused)
                    tip += ". No step shows it.";
                Tooltip(tip.c_str());
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        void StepTrack(CreatureFile& file, PaintState& paint, TimelineState& timeline, float width)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            const int total = std::max(1, SpriteEdit::TotalTicks(sprite));
            const float height = std::round(Px(34.0f));
            const float tick = std::clamp(std::floor(width / static_cast<float>(total)), std::round(Px(3.0f)), std::round(Px(18.0f)));
            const float trackWidth = tick * static_cast<float>(total);
            const bool scrolls = trackWidth > width;
            const float scrollbar = scrolls ? ImGui::GetStyle().ScrollbarSize : 0.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##steps", ImVec2(width, height + scrollbar), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleColor();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 start = ImGui::GetCursorScreenPos();
            float x = start.x;
            for (std::size_t index = 0; index < sprite.Steps.size(); ++index)
            {
                const SpriteFile::Step& step = sprite.Steps[index];
                const float blockWidth = tick * static_cast<float>(std::max(0, step.Ticks));
                ImGui::PushID(static_cast<int>(index));
                ImGui::SetCursorScreenPos(ImVec2(x, start.y));
                if (ImGui::InvisibleButton("##step", ImVec2(std::max(1.0f, blockWidth), height)))
                {
                    timeline.Step = static_cast<int>(index);
                    if (!timeline.Playing && step.Frame >= 0 && step.Frame < static_cast<int>(sprite.Frames.size()))
                        paint.Frame = step.Frame;
                }
                const bool hovered = ImGui::IsItemHovered();
                const bool selected = timeline.Step == static_cast<int>(index);
                const ImVec2 min(x, start.y);
                const ImVec2 max(x + blockWidth - 1.0f, start.y + height);
                const ImVec4 tint = TintOf(step.Frame);
                drawList->AddRectFilled(min, max, Color(ManifoldTheme::WithAlpha(tint, hovered || selected ? 0.42f : 0.26f)), Px(3.0f));
                if (selected)
                    drawList->AddRect(min, max, Color(ManifoldTheme::Accent), Px(3.0f), 0, Px(2.0f));
                std::string text = std::to_string(step.Frame + 1);
                if (step.Blink)
                    text += " " ICON_FA_EYE_SLASH;
                if (blockWidth > ImGui::CalcTextSize(text.c_str()).x + Px(8.0f))
                {
                    drawList->AddText(ImVec2(x + Px(4.0f), start.y + Px(3.0f)), Color(ManifoldTheme::Text), text.c_str());
                    std::string motion;
                    if (step.Lift != 0)
                        motion += "up " + std::to_string(step.Lift);
                    if (step.Shift != 0)
                        motion += std::string(motion.empty() ? "" : ", ") + (step.Shift < 0 ? "left " : "right ") + std::to_string(std::abs(step.Shift));
                    if (!motion.empty() && blockWidth > ImGui::CalcTextSize(motion.c_str()).x + Px(8.0f))
                        drawList->AddText(ImVec2(x + Px(4.0f), start.y + height - ImGui::GetTextLineHeight() - Px(3.0f)), Color(ManifoldTheme::Label), motion.c_str());
                }
                char tip[160];
                std::snprintf(tip, sizeof(tip), "Step %zu: frame %d for %d ticks (%.2f s), up %d, sideways %d%s", index + 1, step.Frame + 1, step.Ticks,
                    step.Ticks * SpriteFile::kTickSeconds, step.Lift, step.Shift, step.Blink ? ", eyes shut" : "");
                Tooltip(tip);
                ImGui::PopID();
                x += blockWidth;
            }
            if (timeline.Playing)
            {
                const double played = std::fmod(PlayedTicks(timeline), static_cast<double>(total));
                const float head = start.x + static_cast<float>(played) * tick;
                drawList->AddLine(ImVec2(head, start.y - Px(2.0f)), ImVec2(head, start.y + height + Px(2.0f)), Color(ManifoldTheme::Text), Px(2.0f));
                if (scrolls)
                    ImGui::SetScrollX(std::max(0.0f, head - start.x - width * 0.5f));
            }
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y));
            ImGui::Dummy(ImVec2(trackWidth, height));
            ImGui::EndChild();
        }

        struct FieldMetrics
        {
            float Button;
            float Row;
            float Gap;
            float TitleGap;
            float Label;
        };

        FieldMetrics Fields()
        {
            return { std::round(Px(22.0f)), std::round(Px(20.0f)), std::round(Px(3.0f)), std::round(Px(7.0f)), std::round(Px(64.0f)) };
        }

        float FieldsHeight()
        {
            const FieldMetrics metrics = Fields();
            return metrics.Button + metrics.TitleGap + metrics.Row * 5.0f + metrics.Gap * 4.0f;
        }

        void StepLabel(const char* label, const char* tip, ImVec2 pos, float height)
        {
            const FieldMetrics metrics = Fields();
            ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)), Color(ManifoldTheme::Muted), label);
            if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + metrics.Label, pos.y + height)))
                ImGui::SetTooltip("%s", tip);
        }

        bool Stepper(const char* id, const char* label, const char* tip, bool enabled, int value, int low, int high, int& result, const char* suffix = nullptr)
        {
            ImGui::PushID(id);
            const FieldMetrics metrics = Fields();
            const float side = metrics.Row;
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            StepLabel(label, tip, pos, side);
            ImGui::SetCursorScreenPos(ImVec2(pos.x + metrics.Label, pos.y));
            bool changed = false;
            if (IconButton("##less", ICON_FA_MINUS, side, ManifoldTheme::Text, false, enabled && value > low))
            {
                result = value - 1;
                changed = true;
            }
            ImGui::SameLine(0.0f, 0.0f);
            const std::string text = enabled ? std::to_string(value) : std::string("-");
            const float valueWidth = std::round(Px(30.0f));
            const ImVec2 valuePos = ImGui::GetCursorScreenPos();
            drawList->AddText(ImVec2(std::round(valuePos.x + (valueWidth - ImGui::CalcTextSize(text.c_str()).x) * 0.5f), std::round(valuePos.y + (side - ImGui::GetTextLineHeight()) * 0.5f)),
                Color(enabled ? ManifoldTheme::Text : ManifoldTheme::Muted), text.c_str());
            ImGui::Dummy(ImVec2(valueWidth, side));
            ImGui::SameLine(0.0f, 0.0f);
            if (IconButton("##more", ICON_FA_PLUS, side, ManifoldTheme::Text, false, enabled && value < high))
            {
                result = value + 1;
                changed = true;
            }
            if (enabled && suffix && *suffix)
            {
                ImGui::SameLine(0.0f, Px(6.0f));
                const ImVec2 suffixPos = ImGui::GetCursorScreenPos();
                drawList->AddText(ImVec2(suffixPos.x, std::round(suffixPos.y + (side - ImGui::GetTextLineHeight()) * 0.5f)), Color(ManifoldTheme::Muted), suffix);
                ImGui::Dummy(ImVec2(ImGui::CalcTextSize(suffix).x, side));
            }
            ImGui::PopID();
            return changed;
        }

        void StepFields(CreatureFile& file, PaintState& paint, TimelineState& timeline, float width)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            const int count = static_cast<int>(sprite.Steps.size());
            const bool chosen = timeline.Step >= 0 && timeline.Step < count;
            const int at = chosen ? timeline.Step : -1;
            const SpriteFile::Step step = chosen ? sprite.Steps[static_cast<std::size_t>(at)] : SpriteFile::Step{};
            const FieldMetrics metrics = Fields();
            const float button = metrics.Button;
            const float gap = Px(2.0f);
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float titleY = std::round(origin.y + (button - ImGui::GetTextLineHeight()) * 0.5f);
            if (chosen)
            {
                const std::string title = "Step " + std::to_string(at + 1) + " of " + std::to_string(count);
                BoldText(drawList, ImVec2(origin.x, titleY), ManifoldTheme::Text, title.c_str());
            }
            else
            {
                constexpr const char* kNone = "No step chosen";
                drawList->AddText(ImVec2(origin.x, titleY), Color(ManifoldTheme::Muted), kNone);
                const ImVec2 size = ImGui::CalcTextSize(kNone);
                if (ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(origin.x, titleY), ImVec2(origin.x + size.x, titleY + size.y)))
                    ImGui::SetTooltip("Click a block in the Steps row to change it here");
            }

            const bool editable = chosen && !timeline.Playing;
            ImGui::SetCursorScreenPos(ImVec2(origin.x + width - button * 4.0f - gap * 3.0f, origin.y));
            if (IconButton("##add_step", ICON_FA_PLUS, button, ManifoldTheme::Text, false, editable))
            {
                SpriteFile::Step added = step;
                added.Blink = false;
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::InsertStep(edited, at + 1, added); }))
                    timeline.Step = at + 1;
            }
            Tooltip("Add a step after this one");
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##step_left", ICON_FA_ARROW_LEFT, button, ManifoldTheme::Text, false, editable && at > 0))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::MoveStep(edited, at, at - 1); }))
                    timeline.Step = at - 1;
            }
            Tooltip("Move this step earlier");
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##step_right", ICON_FA_ARROW_RIGHT, button, ManifoldTheme::Text, false, editable && at + 1 < count))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::MoveStep(edited, at, at + 1); }))
                    timeline.Step = at + 1;
            }
            Tooltip("Move this step later");
            ImGui::SameLine(0.0f, gap);
            if (IconButton("##remove_step", ICON_FA_TRASH_ALT, button, ManifoldTheme::Error, false, editable && count > 1))
            {
                if (Change(file, [&](SpriteFile::Sprite& edited) { return SpriteEdit::RemoveStep(edited, at); }))
                    timeline.Step = std::min(at, count - 2);
            }
            Tooltip("Remove this step");

            const auto rowAt = [&](int row)
                {
                    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + button + metrics.TitleGap + static_cast<float>(row) * (metrics.Row + metrics.Gap)));
                };
            const bool live = chosen && timeline.Step == at;
            const auto set = [&](auto&& apply)
                {
                    Change(file, [&](SpriteFile::Sprite& edited)
                        {
                            SpriteFile::Step& target = edited.Steps[static_cast<std::size_t>(at)];
                            const SpriteFile::Step before = target;
                            apply(target);
                            return !(target == before);
                        });
                };
            const int frames = std::max(1, static_cast<int>(sprite.Frames.size()));
            int value = 0;
            rowAt(0);
            if (Stepper("frame", "Frame", "The frame this step shows", live, step.Frame + 1, 1, frames, value))
            {
                set([&](SpriteFile::Step& target) { target.Frame = value - 1; });
                if (!timeline.Playing)
                    paint.Frame = value - 1;
            }
            char seconds[32];
            std::snprintf(seconds, sizeof(seconds), "%.2f s", step.Ticks * SpriteFile::kTickSeconds);
            rowAt(1);
            if (Stepper("ticks", "Ticks", "How long the step lasts; 8 ticks are one second", live, step.Ticks, 1, 400, value, seconds))
                set([&](SpriteFile::Step& target) { target.Ticks = value; });
            rowAt(2);
            if (Stepper("lift", "Up", "How many cells higher the frame is drawn", live, step.Lift, 0, SpriteFile::kMaxLift, value))
                set([&](SpriteFile::Step& target) { target.Lift = value; });
            rowAt(3);
            if (Stepper("shift", "Sideways", "How many cells to the right the frame is drawn; below zero to the left", live, step.Shift, -SpriteFile::kMaxShift,
                    SpriteFile::kMaxShift, value))
                set([&](SpriteFile::Step& target) { target.Shift = value; });

            rowAt(4);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            StepLabel("Eyes", "Whether the eyes are shut in this step", pos, metrics.Row);
            ImGui::SetCursorScreenPos(ImVec2(pos.x + metrics.Label, pos.y));
            if (IconButton("##blink", ICON_FA_EYE_SLASH, metrics.Row, ManifoldTheme::Text, live && step.Blink, live))
                set([&](SpriteFile::Step& target) { target.Blink = !target.Blink; });
            Tooltip(!live ? "Whether the eyes are shut in this step" : step.Blink ? "The eyes are shut in this step" : "Shut the eyes in this step");

            ImGui::SetCursorScreenPos(origin);
            ImGui::Dummy(ImVec2(width, FieldsHeight()));
        }
    }

    float TimelineCardHeight()
    {
        const ImGuiStyle& style = ImGui::GetStyle();
        const float line = ImGui::GetTextLineHeight();
        const float spacing = style.ItemSpacing.y;
        const float chrome = UI::PxWhole(14.0f, 12.0f).y * 2.0f + line + spacing + Px(4.0f) + spacing;
        const float left = std::round(Px(24.0f)) + spacing + FrameBox().y + style.ScrollbarSize + spacing + line + spacing + std::round(Px(34.0f)) + style.ScrollbarSize;
        return std::ceil(chrome + std::max(left, FieldsHeight()) + Px(2.0f));
    }

    void TogglePlaying(TimelineState& timeline)
    {
        timeline.Playing = !timeline.Playing;
        timeline.Started = ImGui::GetTime();
    }

    void DrawTimelineCard(CreatureFile* file, PaintState& paint, TimelineState& timeline, ImVec2 size)
    {
        BeginCard("##timeline", ICON_FA_FILM, "Timeline", size);
        if (!file || !file->Sprite || file->Sprite->Frames.empty())
        {
            timeline.Playing = false;
            MutedText(file ? "This creature has no frames to show." : "Choose a creature in the Explorer.");
            EndCard();
            return;
        }
        SpriteFile::Sprite& sprite = *file->Sprite;
        if (timeline.Step >= static_cast<int>(sprite.Steps.size()))
            timeline.Step = static_cast<int>(sprite.Steps.size()) - 1;
        ClampFrame(file, paint);

        if (timeline.Playing)
        {
            const int playing = SpriteEdit::StepAtTick(sprite, static_cast<long long>(std::floor(PlayedTicks(timeline))));
            if (playing >= 0)
            {
                const int frame = sprite.Steps[static_cast<std::size_t>(playing)].Frame;
                if (frame >= 0 && frame < static_cast<int>(sprite.Frames.size()) && !paint.Stroking)
                    paint.Frame = frame;
            }
            Animation::KeepFrames();
        }

        Header(*file, timeline);

        const ImVec2 start = ImGui::GetCursorScreenPos();
        const ImVec2 room = ImGui::GetContentRegionAvail();
        const float fieldsWidth = std::round(Px(250.0f));
        const float gap = std::round(Px(18.0f));
        const float left = std::max(Px(200.0f), room.x - fieldsWidth - gap);

        FrameButtons(*file, paint, timeline, left);
        FrameRow(*file, paint, timeline, left);
        Label("Steps", left);
        StepTrack(*file, paint, timeline, left);

        const float fieldsX = start.x + left + gap;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(fieldsX - gap * 0.5f, start.y), ImVec2(fieldsX - gap * 0.5f, start.y + room.y), Color(ManifoldTheme::RowSeparator));
        ImGui::SetCursorScreenPos(ImVec2(fieldsX, start.y));
        ImGui::BeginGroup();
        StepFields(*file, paint, timeline, fieldsWidth);
        ImGui::EndGroup();
        EndCard();
    }
}
