#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        constexpr ImGuiKey kCode[] = { ImGuiKey_UpArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_B, ImGuiKey_A };
        constexpr double kPetSeconds = IdleAnimation::kPetSeconds;
        std::size_t g_codeStep = 0;
        double g_pettedAt = -100.0;
        int g_petStreak = 0;
        PetOutcome g_petOutcome = PetOutcome::Joy;
        std::vector<CreatureId> g_nextFinds;
        CreatureFacts g_nextFacts;
        double g_nextFindsAt = -100.0;
        bool g_showSets = false;

        void ListenForCode(IdlerModel& model)
        {
            for (const ImGuiKey key : { ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_A, ImGuiKey_B })
            {
                if (!ImGui::IsKeyPressed(key, false))
                    continue;
                if (key == kCode[g_codeStep])
                    ++g_codeStep;
                else if (key != ImGuiKey_UpArrow || g_codeStep != 2)
                    g_codeStep = key == kCode[0] ? 1 : 0;
                if (g_codeStep == std::size(kCode))
                {
                    g_codeStep = 0;
                    model.AwardCreature(CreatureId::Bug);
                }
            }
        }

        float RarityGemSize()
        {
            return std::max(1.0f, std::round(Px(1.5f))) * 5.0f;
        }

        void DrawRarityGem(ImDrawList* drawList, ImVec2 at, Rarity rarity)
        {
            const float cell = std::max(1.0f, std::round(Px(1.5f)));
            const ImVec4 color = RarityColor(rarity);
            const auto fill = [&](float left, float top, float right, float bottom, ImU32 tone)
                {
                    drawList->AddRectFilled(ImVec2(at.x + left * cell, at.y + top * cell), ImVec2(at.x + right * cell, at.y + bottom * cell), tone);
                };
            const ImU32 tone = ImGui::GetColorU32(color);
            fill(2.0f, 0.0f, 3.0f, 1.0f, tone);
            fill(1.0f, 1.0f, 4.0f, 2.0f, tone);
            fill(0.0f, 2.0f, 5.0f, 3.0f, tone);
            fill(1.0f, 3.0f, 4.0f, 4.0f, tone);
            fill(2.0f, 4.0f, 3.0f, 5.0f, tone);
            fill(1.0f, 1.0f, 2.0f, 2.0f, IM_COL32(255, 255, 255, 170));
        }

        void CreatureCell(IdlerModel& model, const CreatureInfo& info, ImVec2 size, float pixel)
        {
            const bool found = model.FoundCreatures().test(static_cast<std::size_t>(info.Id));
            ImGui::PushID(static_cast<int>(info.Id));
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + size.x, min.y + size.y);
            if (ImGui::InvisibleButton("##cell", size))
            {
                if (g_creature != info.Id)
                    g_pettedAt = -100.0;
                g_creature = info.Id;
                model.MarkCreatureSeen(info.Id);
            }
            const bool hovered = ImGui::IsItemHovered();
            const bool visible = ImGui::IsItemVisible();
            ImGui::PopID();
            if (!visible)
                return;

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const float rounding = Px(4.0f);
            if (g_creature == info.Id)
                drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.14f)), rounding);
            else if (hovered)
                drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 9), rounding);

            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const ImVec2 at(std::round(min.x + (size.x - picture.x) * 0.5f), min.y + Px(8.0f));
            IdleAnimation::DrawCreature(drawList, at, info.Id, IdleAnimation::Mood::Idling, static_cast<std::uint32_t>(info.Id) + 1u, pixel, found ? IdleAnimation::Look::Normal : IdleAnimation::Look::Unknown);

            const char* name = found ? IdleAnimation::ShownName(info.Id) : "???";
            const float nameWidth = ImGui::CalcTextSize(name).x;
            const float right = max.x - Px(4.0f);
            const ImVec2 namePos(std::max(min.x + Px(4.0f), std::round(min.x + (size.x - nameWidth) * 0.5f)), max.y - ImGui::GetTextLineHeight() - Px(6.0f));
            DrawCutText(drawList, namePos, right, found ? ManifoldTheme::Label : ManifoldTheme::Muted, name);
            DrawRarityGem(drawList, ImVec2(min.x + Px(5.0f), min.y + Px(5.0f)), RarityOf(info.Id));
            if (model.IsNewCreature(info.Id))
            {
                drawList->AddCircleFilled(ImVec2(max.x - Px(8.0f), min.y + Px(8.0f)), Px(3.0f), ImGui::GetColorU32(ManifoldTheme::Accent));
            }
            else if (const int bond = found ? BondLevel(model.CreatureBondHours(info.Id)) : 0; bond > 0)
            {
                const float heart = std::max(1.0f, std::round(Px(1.0f)));
                for (int index = 0; index < bond; ++index)
                    IdleAnimation::DrawHeart(drawList, ImVec2(max.x - Px(4.0f) - heart * 6.0f * static_cast<float>(index + 1) + heart, min.y + Px(5.0f)), heart, 0.9f);
            }
        }

        void RarityCounts(const IdlerModel& model, float right)
        {
            std::array<int, static_cast<std::size_t>(Rarity::Count)> all{};
            std::array<int, static_cast<std::size_t>(Rarity::Count)> found{};
            const CreatureSet& set = model.FoundCreatures();
            for (const CreatureInfo& info : AllCreatures())
            {
                const auto rarity = static_cast<std::size_t>(RarityOf(info.Id));
                ++all[rarity];
                found[rarity] += set.test(static_cast<std::size_t>(info.Id)) ? 1 : 0;
            }

            const float gem = RarityGemSize();
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float y = std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - line);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            float x = right;
            for (int index = static_cast<int>(Rarity::Count) - 1; index >= 0; --index)
            {
                const auto at = static_cast<std::size_t>(index);
                const std::string text = std::to_string(found[at]) + "/" + std::to_string(all[at]);
                const float width = ImGui::CalcTextSize(text.c_str()).x;
                const float left = x - width - gem - Px(4.0f);
                DrawRarityGem(drawList, ImVec2(left, std::round(y + (line - gem) * 0.5f)), static_cast<Rarity>(index));
                drawList->AddText(ImVec2(left + gem + Px(4.0f), y), ImGui::GetColorU32(ManifoldTheme::Muted), text.c_str());
                if (ImGui::IsMouseHoveringRect(ImVec2(left, y), ImVec2(x, y + line)) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
                {
                    ShowTooltip([&]()
                        {
                            ImGui::TextUnformatted(RarityName(static_cast<Rarity>(index)));
                            ImGui::TextDisabled("%d of %d found", found[at], all[at]);
                        });
                }
                x = left - Px(10.0f);
            }
        }

        void CreatureGrid(IdlerModel& model)
        {
            BeginCard("Collection");
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float button = ImGui::GetFrameHeight();
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - button, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + button) * 0.5f)));
                if (IconLink("##home", ICON_FA_HOME, ImVec2(button, button), "Show their home"))
                {
                    model.Options().CreatureHome = true;
                    model.SaveNow();
                }
                ImGui::SetCursorScreenPos(cursor);
                RarityCounts(model, cursor.x + RowWidth() - button - Px(10.0f));
            }
            BeginCardList("##creatures", 0.0f);
            const float gap = Px(4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(gap, gap));
            const ImVec2 room = ImGui::GetContentRegionAvail();

            const auto cellAt = [&](float size) { return ImVec2(std::round(size * 16.0f + Px(28.0f)), std::round(size * 14.0f + Px(32.0f))); };
            const auto columnsAt = [&](float size) { return std::max(1, static_cast<int>((room.x + gap) / (cellAt(size).x + gap))); };
            const float largest = std::round(Px(8.0f));
            float pixel = std::round(Px(3.0f));
            while (pixel < largest)
            {
                const int rows = (static_cast<int>(kCreatureCount) + columnsAt(pixel + 1.0f) - 1) / columnsAt(pixel + 1.0f);
                if (static_cast<float>(rows) * (cellAt(pixel + 1.0f).y + gap) - gap > room.y)
                    break;
                pixel += 1.0f;
            }

            const ImVec2 cell = cellAt(pixel);
            const int columns = columnsAt(pixel);
            int column = 0;
            for (const CreatureInfo& info : AllCreatures())
            {
                if (column > 0)
                    ImGui::SameLine();
                CreatureCell(model, info, cell, pixel);
                column = (column + 1) % columns;
            }
            ImGui::PopStyleVar();
            EndCardList();
            EndCard();
        }

        void CreatureDetail(IdlerModel& model)
        {
            const CreatureInfo& info = InfoOf(g_creature);
            const bool found = model.FoundCreatures().test(static_cast<std::size_t>(info.Id));
            const CreatureFacts facts = found ? CreatureFacts{} : model.CreatureFactsNow();

            BeginCard(found ? IdleAnimation::ShownName(info.Id) : "???");

            BeginRow("##stage");
            const ImVec2 stageSize(RowWidth(), std::round(Px(136.0f)));
            const ImVec2 stageMin = ImGui::GetCursorScreenPos();
            const bool petting = ImGui::GetTime() - g_pettedAt < kPetSeconds;
            if (ImGui::InvisibleButton("##pet", stageSize) && found && !petting)
            {
                const double now = ImGui::GetTime();
                g_petStreak = now - g_pettedAt < 12.0 ? g_petStreak + 1 : 1;
                g_petOutcome = OutcomeOfPet(info.Id, BondLevel(model.CreatureBondHours(info.Id)), g_petStreak);
                g_pettedAt = now;
                model.PetCreature(info.Id);
            }
            if (found)
                ItemTooltip("Click to pet it.");
            ImGui::PopID();

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const Habit habit = IdleAnimation::HabitOf(info.Id);
            const ImVec2 stageMax(stageMin.x + stageSize.x, stageMin.y + stageSize.y);
            const CreatureStage stage = DrawCreatureStage(model, stageMin, stageMax, habit);
            const double petted = ImGui::GetTime() - g_pettedAt;
            const bool reacting = found && petted < kPetSeconds;
            const PetOutcome outcome = reacting ? g_petOutcome : PetOutcome::Shrug;
            const float pixel = std::round(Px(7.0f)) + (reacting && outcome == PetOutcome::Puff ? std::round(Px(1.0f)) : 0.0f);
            const ImVec2 picture = IdleAnimation::SizeAt(pixel);
            const ImVec2 normal = IdleAnimation::SizeAt(std::round(Px(7.0f)));
            float top = std::round(stage.Ground - normal.y);
            if (habit == Habit::Floats)
                top = std::round(std::max(stageMin.y + Px(2.0f), stage.Ground - normal.y + Px(6.0f)));
            else if (habit == Habit::Swims)
                top = std::round((stageMin.y + Px(4.0f) + stage.Ground - normal.y) * 0.5f);
            top -= picture.y - normal.y;
            const float petShare = static_cast<float>(petted / kPetSeconds);
            float hop = 0.0f;
            float aside = 0.0f;
            if (reacting)
            {
                if (outcome == PetOutcome::Joy && petted < kPetSeconds * 0.4)
                    hop = std::round(pixel * 1.5f * static_cast<float>(std::sin(petted / (kPetSeconds * 0.4) * 3.14159265)));
                else if (outcome == PetOutcome::Dodge)
                    aside = std::round(pixel * 3.0f * static_cast<float>(std::sin(std::min(1.0f, petShare * 1.3f) * 3.14159265)));
                else if (outcome == PetOutcome::Bite || outcome == PetOutcome::Puff)
                    aside = std::round(Px(2.0f) * static_cast<float>(std::sin(petted * 45.0)));
            }
            const ImVec2 at(std::round(stageMin.x + (stageSize.x - picture.x) * 0.5f) + aside, top - hop);
            IdleAnimation::DrawCreature(drawList, at, info.Id, IdleAnimation::Mood::Idling, static_cast<std::uint32_t>(info.Id) + 1u, pixel, found ? IdleAnimation::Look::Normal : IdleAnimation::Look::Unknown);
            DrawCreatureStageFront(stageMin, stageMax, habit);
            if (reacting)
            {
                IdleAnimation::RequestFrames();
                const float heartPixel = std::round(Px(2.0f));
                const ImVec2 mark(at.x + picture.x + Px(14.0f), at.y + Px(24.0f));
                switch (outcome)
                {
                case PetOutcome::Joy:
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x - pixel, at.y + pixel * 2.0f - Px(30.0f) * petShare), heartPixel, 1.0f - petShare);
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x + picture.x, at.y + pixel * 3.0f - Px(40.0f) * petShare), heartPixel, 1.0f - petShare);
                    break;
                case PetOutcome::Shrug:
                    IdleAnimation::DrawHeart(drawList, ImVec2(at.x + picture.x, at.y + pixel * 3.0f - Px(24.0f) * petShare), std::round(Px(1.0f)), 1.0f - petShare);
                    break;
                case PetOutcome::Dodge:
                    IdleAnimation::DrawMark(drawList, mark, heartPixel, IdleAnimation::Mark::Dots, 1.0f - petShare * petShare);
                    break;
                case PetOutcome::Bite:
                    IdleAnimation::DrawAngerMark(drawList, mark, pixel * 0.5f, std::fmod(petted, 0.3) < 0.15);
                    break;
                case PetOutcome::Puff:
                    IdleAnimation::DrawMark(drawList, mark, heartPixel, IdleAnimation::Mark::Alarm, 1.0f - petShare * petShare);
                    break;
                }
            }

            const auto wrapped = [](const char* id, const char* text, ImVec4 color)
                {
                    BeginRow(id);
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + RowWidth());
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(text);
                    ImGui::PopStyleColor();
                    ImGui::PopTextWrapPos();
                    ImGui::PopID();
                };
            wrapped("##text", found ? IdleAnimation::ShownFlavour(info.Id) : info.Hint, found ? ManifoldTheme::Text : ManifoldTheme::Label);
            InfoRow("Rarity", RarityName(RarityOf(info.Id)), RarityColor(RarityOf(info.Id)));

            if (found)
            {
                const std::uint32_t foundAt = model.CreatureFoundAt(info.Id);
                wrapped("##how", info.How, ManifoldTheme::Muted);
                if (foundAt != 0)
                    InfoRow("Found", DateText(foundAt), ManifoldTheme::Label);

                const double bondHours = model.CreatureBondHours(info.Id);
                const int bond = BondLevel(bondHours);
                std::string bondText = BondName(bond);
                if (bond < kMaxBondLevel)
                    bondText += ", " + HoursText(static_cast<float>(std::round(bondHours * 10.0) / 10.0)) + " of " + HoursText(static_cast<float>(BondHoursOfLevel(bond + 1)));
                InfoRow("Bond", bondText, bond > 0 ? kBondColor : ManifoldTheme::Muted);
                ItemTooltip("Grows with the hours of the games that show it, and a little with every pet and every game played with it.");
                if (bond < kMaxBondLevel)
                {
                    BeginRow("##bond_bar");
                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - Px(5.0f));
                    const ImVec2 barMin = ImGui::GetCursorScreenPos();
                    const float barWidth = RowWidth();
                    const float barHeight = std::round(Px(3.0f));
                    ImGui::Dummy(ImVec2(barWidth, barHeight));
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(1.5f));
                    if (const float bondShare = BondShare(bondHours); bondShare > 0.0f)
                        drawList->AddRectFilled(barMin, ImVec2(barMin.x + std::max(barHeight, barWidth * bondShare), barMin.y + barHeight), ImGui::GetColorU32(kBondColor), Px(1.5f));
                    ImGui::PopID();
                }

                std::string sets;
                for (const CreatureFamily& family : AllFamilies())
                {
                    if (IsInFamily(family, info.Id))
                        sets += (sets.empty() ? "" : ", ") + std::string(family.Name);
                }
                if (!sets.empty())
                    InfoRow("Set", sets, ManifoldTheme::Label);
                if (EvolutionOf(info.Id))
                {
                    if (model.CreatureEvolved(info.Id))
                    {
                        bool evolved = !model.CreatureInFirstForm(info.Id);
                        if (ToggleRow("Evolved form", "Off shows its first form.", evolved))
                            model.SetCreatureFirstForm(info.Id, !evolved);
                    }
                    else
                        InfoRow("Evolves at", BondName(kEvolveAtBond), ManifoldTheme::Muted);
                }
                bool buddy = model.Buddy() == info.Id;
                if (ToggleRow("At your account", "Stands next to your name in the sidebar while games idle.", buddy))
                    model.SetBuddy(buddy ? std::optional<CreatureId>(info.Id) : std::nullopt);

                const std::uint32_t playLeft = model.PlayBondLeftToday(info.Id);
                InfoRow("Play", playLeft > 0 ? std::to_string(playLeft) + " min of bond left today" : std::string("Today's bond from play is used up"),
                    playLeft > 0 ? ManifoldTheme::Label : ManifoldTheme::Muted);
                ItemTooltip("Every game brings a little bond, up to 30 minutes a day for each creature.");
                BeginRow("##play");
                const MiniGame terrain = TerrainGameOf(habit);
                if (ActionButton(MiniGameIcon(terrain), MiniGameName(terrain)))
                    OpenMiniGame(info.Id, terrain);
                if (ActionButton(MiniGameIcon(MiniGame::ShellGame), MiniGameName(MiniGame::ShellGame)))
                    OpenMiniGame(info.Id, MiniGame::ShellGame);
                EndActionButtons();
                ImGui::PopID();
            }
            else if (const float share = ProgressShare(info, facts); share >= 0.0f)
            {
                BeginRow("##progress");
                const ImVec2 barMin = ImGui::GetCursorScreenPos();
                const float barWidth = RowWidth();
                const float barHeight = std::round(Px(6.0f));
                ImGui::Dummy(ImVec2(barWidth, barHeight));
                drawList->AddRectFilled(barMin, ImVec2(barMin.x + barWidth, barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(3.0f));
                if (share > 0.0f)
                    drawList->AddRectFilled(barMin, ImVec2(barMin.x + std::max(barHeight, barWidth * share), barMin.y + barHeight), ImGui::GetColorU32(ManifoldTheme::Accent), Px(3.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
                ImGui::TextUnformatted(ProgressText(info, facts).c_str());
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            EndCard();
        }

        void SetRows(IdlerModel& model)
        {
            const CreatureSet& found = model.FoundCreatures();
            BeginCardList("##sets", 0.0f);
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = std::round(ImGui::GetTextLineHeight() + Px(5.0f));
            for (const CreatureFamily& family : AllFamilies())
            {
                ImGui::PushID(family.Name);
                const int have = FoundInFamily(family, found);
                const int all = static_cast<int>(family.Members.size());
                const ImVec2 min = ImGui::GetCursorScreenPos();
                const ImVec2 max(min.x + width, min.y + height);
                if (ImGui::InvisibleButton("##set", ImVec2(width, height)))
                {
                    g_creature = family.Members.front();
                    g_pettedAt = -100.0;
                    for (const CreatureId id : family.Members)
                    {
                        if (!found.test(static_cast<std::size_t>(id)))
                        {
                            g_creature = id;
                            break;
                        }
                    }
                }
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (ImGui::IsItemHovered())
                {
                    drawList->AddRectFilled(min, max, IM_COL32(255, 255, 255, 9), Px(4.0f));
                    ShowTooltip([&]()
                        {
                            for (const CreatureId id : family.Members)
                            {
                                if (found.test(static_cast<std::size_t>(id)))
                                    ImGui::TextUnformatted(InfoOf(id).Name);
                                else
                                    ImGui::TextDisabled("???");
                            }
                        });
                }
                const float textY = std::round(min.y + (height - ImGui::GetTextLineHeight()) * 0.5f);
                const bool complete = have == all;
                const std::string count = std::to_string(have) + "/" + std::to_string(all);
                const float countWidth = ImGui::CalcTextSize(count.c_str()).x;
                drawList->AddText(ImVec2(max.x - Px(4.0f) - countWidth, textY), ImGui::GetColorU32(complete ? ManifoldTheme::Accent : ManifoldTheme::Muted), count.c_str());
                DrawCutText(drawList, ImVec2(min.x + Px(4.0f), textY), max.x - Px(12.0f) - countWidth, complete ? ManifoldTheme::Text : ManifoldTheme::Label, family.Name);
                ImGui::PopID();
            }
            EndCardList();
        }

        void NextFindsCard(IdlerModel& model)
        {
            const double now = ImGui::GetTime();
            if (now - g_nextFindsAt > 2.0)
            {
                g_nextFindsAt = now;
                g_nextFacts = model.CreatureFactsNow();
                g_nextFinds = NextFinds(model.FoundCreatures(), g_nextFacts, 3);
            }

            const bool showSets = g_showSets || g_nextFinds.empty();
            BeginCard(showSets ? "Sets" : "Next finds");
            if (!g_nextFinds.empty())
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float button = ImGui::GetFrameHeight();
                ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - button, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + button) * 0.5f)));
                if (IconLink("##sets_switch", showSets ? ICON_FA_HOURGLASS_HALF : ICON_FA_LAYER_GROUP, ImVec2(button, button), showSets ? "Show the next finds" : "Show the sets"))
                    g_showSets = !g_showSets;
                ImGui::SetCursorScreenPos(cursor);
            }
            if (showSets)
            {
                SetRows(model);
                EndCard();
                return;
            }
            for (const CreatureId id : g_nextFinds)
            {
                const CreatureInfo& info = InfoOf(id);
                BeginRow(info.Key);
                const float width = RowWidth();
                const float pixel = IdleAnimation::PixelSize();
                const ImVec2 picture = IdleAnimation::SizeAt(pixel);
                const ImVec2 min = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##find", ImVec2(width, picture.y)))
                {
                    g_creature = id;
                    g_pettedAt = -100.0;
                }
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (ImGui::IsItemHovered())
                    drawList->AddRectFilled(min, ImVec2(min.x + width, min.y + picture.y), IM_COL32(255, 255, 255, 9), Px(4.0f));
                IdleAnimation::DrawCreature(drawList, min, id, IdleAnimation::Mood::Idling, 1u, pixel, IdleAnimation::Look::Unknown);

                const float textX = min.x + picture.x + Px(8.0f);
                const float right = min.x + width - Px(4.0f);
                DrawCutText(drawList, ImVec2(textX, min.y), right, ManifoldTheme::Label, info.Hint);
                const float barTop = min.y + ImGui::GetTextLineHeight() + Px(2.0f);
                const float barHeight = std::round(Px(3.0f));
                const float barWidth = right - textX;
                drawList->AddRectFilled(ImVec2(textX, barTop), ImVec2(right, barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(1.5f));
                const float share = ProgressShare(info, g_nextFacts);
                if (share > 0.0f)
                    drawList->AddRectFilled(ImVec2(textX, barTop), ImVec2(textX + std::max(barHeight, barWidth * share), barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Accent), Px(1.5f));
                DrawCutText(drawList, ImVec2(textX, barTop + barHeight + Px(1.0f)), right, ManifoldTheme::Muted, ProgressText(info, g_nextFacts));
                ImGui::PopID();
            }
            EndCard();
        }
    }

    CreatureId g_creature = CreatureId::Slime;

    ImVec4 RarityColor(Rarity rarity)
    {
        switch (rarity)
        {
        case Rarity::Common: return ImVec4(0.72f, 0.74f, 0.78f, 1.0f);
        case Rarity::Rare: return ImVec4(0.36f, 0.64f, 1.0f, 1.0f);
        case Rarity::Epic: return ImVec4(0.74f, 0.47f, 1.0f, 1.0f);
        case Rarity::Legendary: return ImVec4(1.0f, 0.77f, 0.25f, 1.0f);
        default: return ImVec4(0.38f, 0.84f, 0.45f, 1.0f);
        }
    }

    void CreaturesPage(Shell& app)
    {
        IdlerModel& model = app.Model();
        ListenForCode(model);
        model.MarkCreatureSeen(g_creature);

        const std::size_t found = model.FoundCreatures().count();
        BeginPage(app, std::to_string(found) + " of " + std::to_string(kCreatureCount) + " found");
        if (model.Options().CreatureHome)
        {
            const ImVec2 corner = ImGui::GetCursorScreenPos();
            const ImVec2 room = ImGui::GetContentRegionAvail();
            DrawCreatureHome(model, ImVec2(room.x, std::round(room.y - 1.0f)));
            const float button = ImGui::GetFrameHeight();
            const float margin = Px(8.0f);
            ImGui::SetCursorScreenPos(ImVec2(corner.x + room.x - button - margin, corner.y + margin));
            if (IconLink("##collection", ICON_FA_TH_LARGE, ImVec2(button, button), "Show the collection"))
            {
                model.Options().CreatureHome = false;
                model.SaveNow();
            }
            const bool ball = model.Options().HomeBall;
            ImGui::SetCursorScreenPos(ImVec2(corner.x + room.x - button * 2.0f - margin - Px(4.0f), corner.y + margin));
            if (IconLink("##ball", ICON_FA_BASEBALL_BALL, ImVec2(button, button), ball ? "Put the ball away" : "Bring a ball to throw",
                ball ? ManifoldTheme::AccentHovered : ImVec4(0.77f, 0.78f, 0.80f, 1.0f), ball ? ManifoldTheme::Accent : ImVec4(0.44f, 0.45f, 0.49f, 1.0f)))
            {
                model.Options().HomeBall = !ball;
                model.SaveNow();
            }
            EndPageBody();
            return;
        }
        CardColumns("##creature_columns",
            [&]() { CreatureGrid(model); },
            [&]() { CreatureDetail(model); NextFindsCard(model); }, 0.62f, Px(kDetailColumnWidth));
        EndPageBody();
    }
}
