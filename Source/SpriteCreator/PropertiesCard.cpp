#include "SpriteCreator/PropertiesCard.h"

#include "Core/Errors.h"
#include "Core/Text.h"
#include "Generated/IconsFontAwesome5.h"
#include "SpriteCreator/Widgets.h"
#include "UI/SpriteView.h"
#include "UI/Theme.h"
#include "UI/UiScale.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace Idler::SpriteCreator
{
    namespace
    {
        using UI::Px;

        struct ColorSlot
        {
            const char* Id;
            const char* Label;
            SpriteFile::Color SpriteFile::Sprite::*Member;
            bool MayBeEmpty;
        };

        constexpr ColorSlot kColorSlots[] = {
            { "##body", "Body", &SpriteFile::Sprite::Body, false },
            { "##second", "Second", &SpriteFile::Sprite::Second, true },
            { "##third", "Third", &SpriteFile::Sprite::Third, true },
        };

        bool IsDigit(char c)
        {
            return c >= '0' && c <= '9';
        }

        bool IsLetter(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        }

        int FirstNumberAfter(std::string_view text, std::string_view word)
        {
            for (std::size_t at = text.find(word); at != std::string_view::npos; at = text.find(word, at + 1))
            {
                if (at > 0 && IsLetter(text[at - 1]))
                    continue;
                std::size_t next = at + word.size();
                if (next < text.size() && text[next] == 's')
                    ++next;
                if (next + 1 < text.size() && text[next] == ' ' && IsDigit(text[next + 1]))
                {
                    int number = 0;
                    for (std::size_t digit = next + 1; digit < text.size() && IsDigit(text[digit]); ++digit)
                        number = number * 10 + (text[digit] - '0');
                    return number;
                }
            }
            return -1;
        }

        float LabelWidth()
        {
            return std::round(Px(84.0f));
        }

        void RowLabel(const char* label)
        {
            FieldLabel(label, LabelWidth());
        }

        void FactRow(const char* label, const std::string& value, ImVec4 color = ManifoldTheme::Text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();
            ImGui::SameLine(LabelWidth());
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

        std::uint32_t Packed(const float rgb[3])
        {
            const auto channel = [](float value) { return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); };
            return (channel(rgb[0]) << 16) | (channel(rgb[1]) << 8) | channel(rgb[2]);
        }

        bool Switch(const char* id, bool value, bool enabled)
        {
            const float height = std::round(ImGui::GetFrameHeight() * 0.8f);
            const float width = std::round(height * 1.8f);
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float top = std::round(pos.y + (ImGui::GetFrameHeight() - height) * 0.5f);
            ImGui::BeginDisabled(!enabled);
            const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, ImGui::GetFrameHeight()));
            ImGui::EndDisabled();
            const bool hovered = enabled && ImGui::IsItemHovered();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec4 track = value ? ManifoldTheme::WithAlpha(ManifoldTheme::Accent, enabled ? 0.85f : 0.35f)
                : ManifoldTheme::WithAlpha(ManifoldTheme::Text, hovered ? 0.22f : 0.14f);
            drawList->AddRectFilled(ImVec2(pos.x, top), ImVec2(pos.x + width, top + height), Color(track), height * 0.5f);
            const float knob = height - Px(4.0f);
            const float knobX = value ? pos.x + width - knob - Px(2.0f) : pos.x + Px(2.0f);
            drawList->AddRectFilled(ImVec2(knobX, top + Px(2.0f)), ImVec2(knobX + knob, top + Px(2.0f) + knob),
                Color(enabled ? ManifoldTheme::Text : ManifoldTheme::Muted), knob * 0.5f);
            return clicked && enabled;
        }

        void ColorRow(CreatureFile& file, const ColorSlot& slot, PropertiesState& state)
        {
            const SpriteFile::Color color = (*file.Sprite).*slot.Member;
            RowLabel(slot.Label);
            const float side = ImGui::GetFrameHeight();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton(slot.Id, ImVec2(side, side)))
                ImGui::OpenPopup(slot.Id);
            Tooltip(slot.MayBeEmpty ? "Choose the colour, the accent colour or none" : "Choose the colour or the accent colour");
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 max(pos.x + side, pos.y + side);
            if (color.Kind == SpriteFile::ColorKind::None)
            {
                drawList->AddRect(pos, max, Color(ManifoldTheme::Border), Px(4.0f));
                drawList->AddLine(ImVec2(pos.x + Px(3.0f), max.y - Px(3.0f)), ImVec2(max.x - Px(3.0f), pos.y + Px(3.0f)), Color(ManifoldTheme::Muted));
            }
            else
            {
                drawList->AddRectFilled(pos, max, Color(UI::ColorOf(color)), Px(4.0f));
            }
            if (ImGui::IsItemHovered())
                drawList->AddRect(pos, max, Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.6f)), Px(4.0f));
            ImGui::SameLine(0.0f, Px(8.0f));
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, color.Kind == SpriteFile::ColorKind::None ? ManifoldTheme::Muted : ManifoldTheme::Text);
            ImGui::TextUnformatted(ColorName(color).c_str());
            ImGui::PopStyleColor();

            if (ImGui::BeginPopup(slot.Id))
            {
                const ImVec4 shown = color.Kind == SpriteFile::ColorKind::None ? ManifoldTheme::Muted : UI::ColorOf(color);
                float rgb[3] = { shown.x, shown.y, shown.z };
                ImGui::SetNextItemWidth(std::round(Px(220.0f)));
                const bool changed = ImGui::ColorPicker3("##picker", rgb,
                    ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_PickerHueBar);
                const bool active = ImGui::IsItemActive();
                if ((active || changed) && !state.ColorDrag)
                {
                    Remember(file);
                    state.ColorDrag = true;
                }
                if (changed)
                {
                    (*file.Sprite).*slot.Member = { SpriteFile::ColorKind::Rgb, Packed(rgb) };
                    Refresh(file);
                }
                if (!active && state.ColorDrag)
                {
                    Forget(file);
                    state.ColorDrag = false;
                }
                if (ImGui::Button("Accent colour"))
                {
                    Change(file, [&](SpriteFile::Sprite& edited)
                        {
                            SpriteFile::Color& target = edited.*slot.Member;
                            const bool differs = target.Kind != SpriteFile::ColorKind::Accent;
                            target = { SpriteFile::ColorKind::Accent, 0 };
                            return differs;
                        });
                }
                if (slot.MayBeEmpty)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("No colour"))
                    {
                        Change(file, [&](SpriteFile::Sprite& edited)
                            {
                                SpriteFile::Color& target = edited.*slot.Member;
                                const bool differs = target.Kind != SpriteFile::ColorKind::None;
                                target = {};
                                return differs;
                            });
                    }
                }
                ImGui::EndPopup();
            }
        }

        void ChangeMotion(CreatureFile& file, CreatureFile* evolved, const Motion& motion)
        {
            Change(file, [&](SpriteFile::Sprite& edited) { return SetMotion(edited, motion); });
            if (evolved && evolved->Sprite)
                Change(*evolved, [&](SpriteFile::Sprite& edited) { return SetMotion(edited, motion); });
        }

        void MotionRows(CreatureFile& file, CreatureFile* counterpart)
        {
            const SpriteFile::Sprite& sprite = *file.Sprite;
            const bool evolved = file.Evolved;
            CreatureFile* follower = !evolved ? counterpart : nullptr;
            const Motion current = MotionOf(sprite);
            const char* lockedTip = "An evolved form moves and faces like its first form; change it there";

            RowLabel("Faces");
            const struct
            {
                const char* Id;
                const char* Label;
                int Facing;
            } faces[] = { { "##left", "Left", -1 }, { "##ahead", "Ahead", 0 }, { "##right", "Right", 1 } };
            for (std::size_t index = 0; index < std::size(faces); ++index)
            {
                if (index > 0)
                    ImGui::SameLine(0.0f, Px(4.0f));
                if (Segment(faces[index].Id, faces[index].Label, current.Facing == faces[index].Facing, !evolved))
                {
                    Motion next = current;
                    next.Facing = faces[index].Facing;
                    ChangeMotion(file, follower, next);
                }
                Tooltip(evolved ? lockedTip : "The way the picture looks; the idler mirrors it when the creature walks the other way");
            }

            const struct
            {
                const char* Id;
                const char* Label;
                const char* Tip;
                bool Motion::*Member;
            } flags[] = {
                { "##floats", "Floats", "Hovers or flies and need not stand on the bottom row", &Motion::Floats },
                { "##swims", "Swims", "Lives in the lake of the terrarium", &Motion::Swims },
                { "##stays", "Stays", "Keeps to one place instead of walking around", &Motion::Stays },
            };
            for (const auto& flag : flags)
            {
                RowLabel(flag.Label);
                if (Switch(flag.Id, current.*flag.Member, !evolved))
                {
                    Motion next = current;
                    next.*flag.Member = !(current.*flag.Member);
                    ChangeMotion(file, follower, next);
                }
                Tooltip(evolved ? lockedTip : flag.Tip);
            }

            RowLabel("No eyes");
            if (Switch("##object", sprite.ObjectLike, true))
            {
                Change(file, [](SpriteFile::Sprite& edited)
                    {
                        edited.ObjectLike = !edited.ObjectLike;
                        return true;
                    });
            }
            Tooltip("Has no eyes on purpose, so the checks do not ask for them");

            if (counterpart && counterpart->Sprite && !(MotionOf(*counterpart->Sprite) == current))
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Warning);
                ImGui::TextWrapped(ICON_FA_EXCLAMATION_TRIANGLE " %s", evolved ? "Moves or faces differently from its first form." : "Its evolved form moves or faces differently.");
                ImGui::PopStyleColor();
                if (ImGui::Button(evolved ? "Take them from the first form" : "Give the evolved form the same"))
                {
                    if (evolved)
                        Change(file, [&](SpriteFile::Sprite& edited) { return SetMotion(edited, MotionOf(*counterpart->Sprite)); });
                    else
                        Change(*counterpart, [&](SpriteFile::Sprite& edited) { return SetMotion(edited, current); });
                }
            }
        }
    }

    std::string ShownFinding(std::string_view text)
    {
        std::string out;
        out.reserve(text.size() + 8);
        std::string word;
        bool lastConverted = false;
        bool inList = false;
        std::size_t index = 0;
        while (index < text.size())
        {
            const char c = text[index];
            if (IsDigit(c))
            {
                std::size_t end = index;
                int number = 0;
                while (end < text.size() && IsDigit(text[end]))
                    number = number * 10 + (text[end++] - '0');
                const bool keyword = word == "frame" || word == "frames" || word == "step" || word == "row" || word == "column";
                const bool pair = (word == "and" || word == "to") && lastConverted;
                const bool partOfNumber = index > 0 && (text[index - 1] == '.' || text[index - 1] == '-');
                if ((keyword || pair || inList) && !partOfNumber)
                {
                    out += std::to_string(number + 1);
                    lastConverted = true;
                }
                else
                {
                    out.append(text.substr(index, end - index));
                    lastConverted = false;
                }
                word.clear();
                index = end;
                continue;
            }
            if (IsLetter(c))
            {
                std::size_t end = index;
                while (end < text.size() && IsLetter(text[end]))
                    ++end;
                word.assign(text.substr(index, end - index));
                out.append(word);
                index = end;
                continue;
            }
            if (c == '[')
                inList = word == "frames";
            else if (c == ']')
                inList = false;
            if (c != ' ')
                word.clear();
            out.push_back(c);
            ++index;
        }
        return out;
    }

    PropertiesRequests DrawPropertiesCard(CreatureFile* file, CreatureFile* counterpart, PropertiesState& state, ImVec2 size)
    {
        PropertiesRequests requests;
        if (state.ColorDrag && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
        {
            if (file)
                Forget(*file);
            state.ColorDrag = false;
        }

        BeginCard("##properties", ICON_FA_SLIDERS_H, "Properties", size);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##properties_body", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleColor();
        if (!file)
        {
            MutedText("Choose a creature in the Explorer.");
            ImGui::EndChild();
            EndCard();
            return requests;
        }

        const ImVec2 at = ImGui::GetCursorScreenPos();
        BoldText(ImGui::GetWindowDrawList(), at, ManifoldTheme::Text, file->Name.c_str());
        ImGui::Dummy(ImVec2(BoldWidth(file->Name.c_str()), ImGui::GetTextLineHeight()));
        FactRow("Key", file->Key, ManifoldTheme::Label);
        FactRow("File", WideToUtf8(file->Path.filename().wstring()) + (file->Saved ? "" : ", not written yet"), ManifoldTheme::Label);
        if (!file->Id)
            MutedText("The idler does not know this creature yet. It needs an entry in Source/Core/Creatures.h and a place in the lists of forge.py; forge.py header then writes it into the program.");

        if (file->Sprite)
        {
            const SpriteFile::Sprite& sprite = *file->Sprite;
            char loop[48];
            std::snprintf(loop, sizeof(loop), "%zu, %.1f s", sprite.Steps.size(), SpriteFile::LoopSeconds(sprite));
            FactRow("Frames", std::to_string(sprite.Frames.size()));
            FactRow("Steps", loop);
            ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
            for (const ColorSlot& slot : kColorSlots)
                ColorRow(*file, slot, state);
            ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
            MotionRows(*file, counterpart);
        }

        ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
        ImGui::TextUnformatted("Checks");
        ImGui::PopStyleColor();
        if (!file->Sprite)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Error);
            ImGui::TextWrapped(ICON_FA_TIMES_CIRCLE " %s", ErrorMessage(ErrorCode::SpriteNotRead, file->Problem).c_str());
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
            const auto finding = [&](const std::string& text, bool error)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, error ? ManifoldTheme::Error : ManifoldTheme::Warning);
                    ImGui::TextWrapped("%s %s", error ? ICON_FA_TIMES_CIRCLE : ICON_FA_EXCLAMATION_TRIANGLE, ShownFinding(text).c_str());
                    ImGui::PopStyleColor();
                    const int frame = FirstNumberAfter(text, "frame");
                    const int step = FirstNumberAfter(text, "step");
                    if ((frame >= 0 || step >= 0) && ImGui::IsItemHovered())
                    {
                        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Color(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.25f)), Px(3.0f));
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            requests.JumpFrame = frame;
                            requests.JumpStep = step;
                        }
                    }
                };
            for (const std::string& error : file->Findings.Errors)
                finding(error, true);
            for (const std::string& warning : file->Findings.Warnings)
                finding(warning, false);
        }
        ImGui::EndChild();
        EndCard();
        return requests;
    }
}
