#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        std::uint64_t g_achievementIconsSeen = 0;

        void AchievementsToolRow(IdlerModel& model, const std::vector<AchievementRow>& rows, bool writing)
        {
            std::size_t settable = 0;
            std::size_t hidden = 0;
            bool server = false;
            std::vector<std::string> locked;
            for (const AchievementRow& row : rows)
            {
                server |= row.Access == AchievementAccess::Server;
                if (row.Access != AchievementAccess::Here)
                    continue;
                ++settable;
                if (!row.Achieved)
                {
                    locked.push_back(row.Id);
                    hidden += row.Hidden ? 1 : 0;
                }
            }

            std::string text;
            ImVec4 color = ManifoldTheme::Muted;
            if (writing)
            {
                text = "Saving on Steam ...";
            }
            else if (!model.AchievementsNotice().empty())
            {
                text = model.AchievementsNotice();
                color = model.AchievementsNoticeIsProblem() ? ManifoldTheme::Error : ManifoldTheme::Label;
            }
            else if (settable == 0)
            {
                text = server ? "Only the game's own server can unlock these." : "Steam's file does not say who may unlock these.";
            }
            else
            {
                text = "Point at one to unlock or lock it.";
            }

            const ImGuiStyle& style = ImGui::GetStyle();
            const char* undoLabel = ICON_FA_UNDO " Undo";
            const char* allLabel = ICON_FA_UNLOCK " Unlock all";
            const bool undo = model.UndoableUnlocks() > 0;
            const bool all = !locked.empty();
            float buttons = 0.0f;
            if (undo)
                buttons += ImGui::CalcTextSize(undoLabel).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
            if (all)
                buttons += ImGui::CalcTextSize(allLabel).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;

            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float height = ImGui::GetFrameHeight();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(pos, ImVec2(std::max(pos.x, pos.x + width - buttons), pos.y + height), true);
            drawList->AddText(ImVec2(pos.x, std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)), ImGui::GetColorU32(color), text.c_str());
            drawList->PopClipRect();
            ImGui::Dummy(ImVec2(std::max(0.0f, width - buttons), height));
            ErrorTooltip(text);

            ImGui::BeginDisabled(writing);
            if (undo)
            {
                ImGui::SameLine();
                if (ImGui::Button(undoLabel))
                    model.UndoUnlocks();
                ItemTooltip("Locks again what Unlock all just unlocked.");
            }
            if (all)
            {
                ImGui::SameLine();
                if (PositiveButton(allLabel))
                    ImGui::OpenPopup("##unlock_all");
            }
            ImGui::EndDisabled();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            if (ImGui::BeginPopup("##unlock_all"))
            {
                const bool one = locked.size() == 1;
                if (one)
                    ImGui::TextUnformatted(hidden > 0 ? "Unlock the last locked achievement? It is a hidden one." : "Unlock the last locked achievement?");
                else
                    ImGui::Text("Unlock the %d locked achievements?", static_cast<int>(locked.size()));
                if (!one && hidden > 0)
                    ImGui::TextDisabled("%d of them are hidden ones.", static_cast<int>(hidden));
                ImGui::TextDisabled(one ? "It appears on your profile with today's date." : "They appear on your profile with today's date.");
                ImGui::TextDisabled(one ? "Undo locks it again while this list is open." : "Undo locks them again while this list is open.");
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
                if (PositiveButton(ICON_FA_UNLOCK " Unlock all"))
                {
                    model.ChangeAchievements(locked, {}, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
            ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
        }

        bool AchievementsRead(IdlerModel& model, std::uint32_t appId, const char* readLabel)
        {
            if (model.AchievementsPhase() == AchievementPhase::Idle && model.StateOf(appId) == RunState::Running)
                model.LoadAchievements(appId);
            const AchievementPhase phase = model.AchievementsPhase();
            if (phase == AchievementPhase::Idle)
            {
                ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
                ImGui::TextUnformatted("Steam only gives achievements and stats to a running game, so reading them counts as a short start of this one: friends can see it for a moment, and its last played date may change.");
                ImGui::PopTextWrapPos();
                ImGui::Dummy(ImVec2(0.0f, Px(4.0f)));
                if (ImGui::Button(readLabel))
                    model.LoadAchievements(appId);
                return false;
            }
            if (phase == AchievementPhase::Busy)
            {
                ImGui::TextDisabled("Asking Steam ...");
                return false;
            }
            if (phase == AchievementPhase::Failed)
            {
                ImGui::TextColored(ManifoldTheme::Error, "%s", model.AchievementsProblem().c_str());
                ErrorTooltip(model.AchievementsProblem());
                if (ImGui::Button(ICON_FA_SYNC " Try again"))
                    model.LoadAchievements(appId);
                return false;
            }
            return true;
        }

        std::optional<std::pair<std::string, float>> ProgressOf(const IdlerModel& model, const AchievementRow& row)
        {
            if (row.Achieved || row.ProgressStat.empty() || row.ProgressMax <= row.ProgressMin)
                return std::nullopt;
            const StatRow* stat = model.FindStatRow(row.ProgressStat);
            if (!stat)
                return std::nullopt;
            const double value = std::clamp(stat->Value, row.ProgressMin, row.ProgressMax);
            const StatKind kind = stat->Schema.Kind;
            return std::make_pair(NumberText(value, kind) + " / " + NumberText(row.ProgressMax, kind),
                static_cast<float>((value - row.ProgressMin) / (row.ProgressMax - row.ProgressMin)));
        }

        void AchievementsSection(IdlerModel& model, std::uint32_t appId)
        {
            if (!AchievementsRead(model, appId, ICON_FA_TROPHY " Read the list"))
                return;
            const auto& rows = model.AchievementList();
            const AchievementPhase phase = model.AchievementsPhase();

            if (rows.empty())
            {
                ImGui::TextDisabled("This game has no achievements.");
            }
            else
            {
                const bool writing = phase == AchievementPhase::Writing;
                AchievementsToolRow(model, rows, writing);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::BeginChild("##achievement_list");
                ImGui::PopStyleColor();

                if (g_achievementIconsSeen != model.AchievementIconsGeneration())
                {
                    g_achievementIconsSeen = model.AchievementIconsGeneration();
                    Covers::RetryFiles();
                }

                const float line = ImGui::GetTextLineHeight();
                const float rowHeight = std::round(line * 2.0f + Px(14.0f));
                const float iconSize = rowHeight - std::round(Px(4.0f));
                const float width = ImGui::GetContentRegionAvail().x;
                const float button = ImGui::GetFrameHeight();
                const bool anySettable = std::any_of(rows.begin(), rows.end(), [](const AchievementRow& row) { return row.Access == AchievementAccess::Here; });
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(rows.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
                while (clipper.Step())
                {
                    for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                    {
                        const AchievementRow& row = rows[static_cast<std::size_t>(index)];
                        const ImVec2 pos = ImGui::GetCursorScreenPos();
                        const ImVec2 rowMax(pos.x + width, pos.y + rowHeight);
                        const bool pointed = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, rowMax);
                        const bool settable = row.Access == AchievementAccess::Here;
                        ImGui::PushID(index);

                        const bool showButton = settable && pointed;
                        if (showButton)
                        {
                            ImGui::SetCursorScreenPos(ImVec2(rowMax.x - button - Px(2.0f), std::round(pos.y + (rowHeight - button) * 0.5f)));
                            ImGui::BeginDisabled(writing);
                            const bool achieved = row.Achieved;
                            if (achieved ? IconButton(ICON_FA_LOCK "##set", button, true) : PositiveIconButton(ICON_FA_UNLOCK "##set", button))
                            {
                                if (achieved)
                                    model.ChangeAchievements({}, { row.Id });
                                else
                                    model.ChangeAchievements({ row.Id }, {});
                            }
                            ImGui::EndDisabled();
                            ItemTooltip(achieved ? "Lock it again on Steam" : "Unlock it on Steam");
                            ImGui::SetCursorScreenPos(pos);
                        }
                        ImGui::Dummy(ImVec2(width, rowHeight));
                        ImGui::PopID();

                        const bool secret = row.Hidden && !row.Achieved;
                        const char* name = secret ? "Hidden achievement" : row.Name.c_str();
                        const char* description = secret ? "" : row.Description.c_str();

                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        const float rowTop = std::round(pos.y + (rowHeight - line * 2.0f - Px(2.0f)) * 0.5f);

                        const ImVec2 iconMin(pos.x + Px(2.0f), std::round(pos.y + (rowHeight - iconSize) * 0.5f));
                        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
                        const ImTextureID icon = row.Icon.empty() ? ImTextureID{} : Covers::File(model.AchievementIconFile(row.Icon), static_cast<int>(iconSize), static_cast<int>(iconSize));
                        if (icon)
                        {
                            drawList->AddImageRounded(icon, iconMin, iconMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), row.Achieved ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 120), Px(4.0f));
                        }
                        else
                        {
                            drawList->AddRectFilled(iconMin, iconMax, ImGui::GetColorU32(ManifoldTheme::Sidebar), Px(4.0f));
                            const char* sign = row.Achieved ? ICON_FA_CHECK : ICON_FA_LOCK;
                            const ImVec2 signSize = ImGui::CalcTextSize(sign);
                            drawList->AddText(ImVec2(std::round(iconMin.x + (iconSize - signSize.x) * 0.5f), std::round(iconMin.y + (iconSize - signSize.y) * 0.5f)), ImGui::GetColorU32(row.Achieved ? ManifoldTheme::Accent : ManifoldTheme::Muted), sign);
                        }
                        float right = pos.x + width - Px(4.0f);
                        if (showButton)
                        {
                            right -= button + Px(8.0f);
                        }
                        else if (row.Achieved && row.UnlockTime != 0)
                        {
                            const std::string date = DateText(row.UnlockTime);
                            right -= ImGui::CalcTextSize(date.c_str()).x;
                            drawList->AddText(ImVec2(right, rowTop), ImGui::GetColorU32(ManifoldTheme::Muted), date.c_str());
                            right -= Px(10.0f);
                        }
                        else if (const auto progress = (row.Hidden ? std::nullopt : ProgressOf(model, row)))
                        {
                            const float barWidth = std::max(ImGui::CalcTextSize(progress->first.c_str()).x, std::round(Px(64.0f)));
                            right -= barWidth;
                            const float textWidth = ImGui::CalcTextSize(progress->first.c_str()).x;
                            drawList->AddText(ImVec2(right + barWidth - textWidth, rowTop), ImGui::GetColorU32(ManifoldTheme::Label), progress->first.c_str());
                            const float barTop = std::round(rowTop + line + Px(6.0f));
                            const float barHeight = std::round(Px(4.0f));
                            drawList->AddRectFilled(ImVec2(right, barTop), ImVec2(right + barWidth, barTop + barHeight), ImGui::GetColorU32(ManifoldTheme::Sidebar), barHeight);
                            if (progress->second > 0.0f)
                                drawList->AddRectFilled(ImVec2(right, barTop), ImVec2(right + std::max(barHeight, std::round(barWidth * progress->second)), barTop + barHeight),
                                    ImGui::GetColorU32(ManifoldTheme::Accent), barHeight);
                            right -= Px(10.0f);
                        }
                        const float textX = iconMax.x + Px(10.0f);
                        drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), pos.y + rowHeight), true);
                        drawList->AddText(ImVec2(textX, rowTop), ImGui::GetColorU32(row.Achieved ? ManifoldTheme::Text : ManifoldTheme::Label), name);
                        drawList->AddText(ImVec2(textX, rowTop + line + Px(2.0f)), ImGui::GetColorU32(ManifoldTheme::Muted), description);
                        drawList->PopClipRect();

                        if (!settable && anySettable)
                            ItemTooltip(row.Access == AchievementAccess::Server ? "Only the game's own server can set this one." : "Steam's file does not say who may set this one.");
                        else if (description[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip) && textX + ImGui::CalcTextSize(description).x > right)
                            ItemTooltip(description);
                    }
                }
                clipper.End();
                ImGui::EndChild();
            }
        }

        std::string StatDetail(const StatRow& stat, int counting)
        {
            std::string detail = stat.Schema.Label.empty() ? std::string() : stat.Schema.Name;
            const auto add = [&](const std::string& part) { detail += (detail.empty() ? "" : "  \xc2\xb7  ") + part; };
            const StatKind kind = stat.Schema.Kind;
            if (stat.Schema.Min && stat.Schema.Max)
                add(NumberText(*stat.Schema.Min, kind) + " to " + NumberText(*stat.Schema.Max, kind));
            else if (stat.Schema.Min)
                add("at least " + NumberText(*stat.Schema.Min, kind));
            else if (stat.Schema.Max)
                add("at most " + NumberText(*stat.Schema.Max, kind));
            if (kind == StatKind::Float)
                add("decimal");
            if (stat.Schema.IncrementOnly)
                add("only counts up");
            if (counting > 0)
                add(counting == 1 ? "counts toward 1 achievement" : "counts toward " + std::to_string(counting) + " achievements");
            return detail;
        }

        void StartEditingStat(const StatRow& stat)
        {
            g_gameWindow.EditingStat = stat.Schema.Name;
            const std::string text = NumberText(stat.Value, stat.Schema.Kind);
            std::snprintf(g_gameWindow.StatText, sizeof(g_gameWindow.StatText), "%s", text.c_str());
            g_gameWindow.FocusStat = true;
        }

        void GameStatsSection(IdlerModel& model, std::uint32_t appId)
        {
            if (!AchievementsRead(model, appId, ICON_FA_LIST_OL " Read the stats"))
                return;
            const std::vector<StatRow>& stats = model.StatList();
            if (stats.empty())
            {
                ImGui::TextDisabled("This game keeps no stats on Steam.");
                return;
            }
            const bool writing = model.AchievementsPhase() == AchievementPhase::Writing;
            const bool anySettable = std::any_of(stats.begin(), stats.end(), [](const StatRow& row) { return row.Settable; });
            if (!model.FindStatRow(g_gameWindow.EditingStat))
                g_gameWindow.EditingStat.clear();

            {
                std::string text;
                ImVec4 color = ManifoldTheme::Muted;
                if (writing)
                {
                    text = "Saving on Steam ...";
                }
                else if (!model.AchievementsNotice().empty())
                {
                    text = model.AchievementsNotice();
                    color = model.AchievementsNoticeIsProblem() ? ManifoldTheme::Error : ManifoldTheme::Label;
                }
                else
                {
                    const bool server = std::any_of(stats.begin(), stats.end(), [](const StatRow& row) { return (row.Schema.Permission & 3u) != 0; });
                    text = anySettable ? "Click a value to change it." : server ? "Only the game's own server can set these." : "The game works these out by itself.";
                }
                const float height = ImGui::GetFrameHeight();
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, std::round(pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f)), ImGui::GetColorU32(color), text.c_str());
                ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, height));
                ErrorTooltip(text);
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
            }

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##stat_list");
            ImGui::PopStyleColor();

            const auto& achievements = model.AchievementList();
            const float line = ImGui::GetTextLineHeight();
            const float rowHeight = std::round(line * 2.0f + Px(14.0f));
            const float width = ImGui::GetContentRegionAvail().x;
            const float button = ImGui::GetFrameHeight();
            const float inputWidth = std::round(Px(150.0f));
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(stats.size()), rowHeight + ImGui::GetStyle().ItemSpacing.y);
            if (!g_gameWindow.EditingStat.empty())
            {
                const auto editing = std::find_if(stats.begin(), stats.end(), [](const StatRow& row) { return row.Schema.Name == g_gameWindow.EditingStat; });
                clipper.IncludeItemByIndex(static_cast<int>(editing - stats.begin()));
            }
            std::optional<std::pair<std::string, double>> setTo;
            while (clipper.Step())
            {
                for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                {
                    const StatRow& stat = stats[static_cast<std::size_t>(index)];
                    const StatKind kind = stat.Schema.Kind;
                    const ImVec2 pos = ImGui::GetCursorScreenPos();
                    const ImVec2 rowMax(pos.x + width, pos.y + rowHeight);
                    const bool editing = g_gameWindow.EditingStat == stat.Schema.Name;
                    const bool pointed = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, rowMax);
                    const float rowTop = std::round(pos.y + (rowHeight - line * 2.0f - Px(2.0f)) * 0.5f);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    ImGui::PushID(index);

                    int counting = 0;
                    for (const AchievementRow& achievement : achievements)
                        counting += achievement.ProgressStat == stat.Schema.Name ? 1 : 0;

                    std::string hint;
                    ImVec4 hintColor = ManifoldTheme::Muted;
                    float right = pos.x + width - Px(4.0f);
                    if (editing)
                    {
                        const float y = std::round(pos.y + (rowHeight - button) * 0.5f);
                        const float cancelX = rowMax.x - button - Px(2.0f);
                        const float setX = cancelX - button - Px(4.0f);
                        const float inputX = setX - inputWidth - Px(6.0f);
                        ImGui::BeginDisabled(writing);
                        ImGui::SetCursorScreenPos(ImVec2(inputX, y));
                        ImGui::SetNextItemWidth(inputWidth);
                        if (g_gameWindow.FocusStat)
                        {
                            ImGui::SetKeyboardFocusHere();
                            g_gameWindow.FocusStat = false;
                        }
                        const bool entered = ImGui::InputText("##value", g_gameWindow.StatText, sizeof(g_gameWindow.StatText),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsScientific);
                        const bool escaped = ImGui::IsItemDeactivated() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);

                        const std::optional<double> value = ParseNumber(g_gameWindow.StatText);
                        std::string problem;
                        if (!value)
                            problem = Trim(g_gameWindow.StatText).empty() ? std::string() : std::string("That is no number.");
                        else
                            problem = StatChangeProblem(stat.Schema, stat.Value, *value);
                        const bool same = value && problem.empty() && NumberText(*value, kind) == NumberText(stat.Value, kind);
                        if (!problem.empty())
                        {
                            hint = problem;
                            hintColor = ManifoldTheme::Error;
                        }
                        else if (value && !same)
                        {
                            int unlocks = 0;
                            for (const AchievementRow& achievement : achievements)
                            {
                                if (!achievement.Achieved && achievement.ProgressStat == stat.Schema.Name && achievement.ProgressMax > achievement.ProgressMin && *value >= achievement.ProgressMax)
                                    ++unlocks;
                            }
                            if (unlocks > 0)
                            {
                                hint = unlocks == 1 ? "Steam unlocks 1 achievement with this." : "Steam unlocks " + std::to_string(unlocks) + " achievements with this.";
                                hintColor = ManifoldTheme::Accent;
                            }
                            else if (stat.Schema.IncrementOnly && *value > stat.Value)
                            {
                                hint = "It only counts up: it cannot go back to " + NumberText(stat.Value, kind) + " afterwards.";
                                hintColor = ManifoldTheme::Warning;
                            }
                            else
                            {
                                hint = "Enter sets it on Steam, Esc keeps " + NumberText(stat.Value, kind) + ".";
                            }
                        }
                        else
                        {
                            hint = "Enter sets it on Steam, Esc keeps " + NumberText(stat.Value, kind) + ".";
                        }

                        const bool ready = value && problem.empty() && !same;
                        ImGui::SetCursorScreenPos(ImVec2(setX, y));
                        ImGui::BeginDisabled(!ready);
                        const bool setClicked = PositiveIconButton(ICON_FA_CHECK "##set", button);
                        ImGui::EndDisabled();
                        ItemTooltip("Set it on Steam");
                        ImGui::SetCursorScreenPos(ImVec2(cancelX, y));
                        const bool cancelClicked = IconButton(ICON_FA_TIMES "##cancel", button);
                        ItemTooltip("Keep the value it has");
                        ImGui::EndDisabled();

                        if ((entered || setClicked) && ready)
                            setTo = std::make_pair(stat.Schema.Name, *value);
                        else if (escaped || cancelClicked)
                            g_gameWindow.EditingStat.clear();
                        if (entered || escaped)
                            ImGui::SetNavCursorVisible(false);
                        right = inputX - Px(10.0f);
                        ImGui::SetCursorScreenPos(pos);
                        ImGui::Dummy(ImVec2(width, rowHeight));
                    }
                    else
                    {
                        const std::string value = NumberText(stat.Value, kind);
                        const float valueWidth = ImGui::CalcTextSize(value.c_str()).x;
                        if (stat.Settable)
                        {
                            ImGui::BeginDisabled(writing);
                            if (ImGui::InvisibleButton("##row", ImVec2(width, rowHeight)))
                                StartEditingStat(stat);
                            ImGui::EndDisabled();
                            if (pointed && !writing)
                            {
                                const char* pen = ICON_FA_PEN;
                                const float penWidth = ImGui::CalcTextSize(pen).x;
                                const ImVec2 frameMin(right - valueWidth - penWidth - Px(18.0f), std::round(pos.y + (rowHeight - button) * 0.5f));
                                const ImVec2 frameMax(right + Px(4.0f), frameMin.y + button);
                                drawList->AddRectFilled(frameMin, frameMax, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
                                drawList->AddText(ImVec2(frameMin.x + Px(8.0f), std::round(frameMin.y + (button - line) * 0.5f)), ImGui::GetColorU32(ManifoldTheme::Muted), pen);
                            }
                        }
                        else
                        {
                            ImGui::Dummy(ImVec2(width, rowHeight));
                        }
                        const float valueY = std::round(pos.y + (rowHeight - line) * 0.5f);
                        drawList->AddText(ImVec2(right - valueWidth, valueY), ImGui::GetColorU32(stat.Settable ? ManifoldTheme::Text : ManifoldTheme::Label), value.c_str());
                        right -= valueWidth + Px(10.0f);
                        if (!stat.Settable)
                        {
                            const char* lock = ICON_FA_LOCK;
                            right -= ImGui::CalcTextSize(lock).x;
                            drawList->AddText(ImVec2(right, valueY), ImGui::GetColorU32(ManifoldTheme::Muted), lock);
                            right -= Px(10.0f);
                        }
                        else if (pointed && !writing)
                        {
                            right -= ImGui::CalcTextSize(ICON_FA_PEN).x + Px(14.0f);
                        }
                    }
                    ImGui::PopID();

                    const std::string label = stat.Schema.Label.empty() ? stat.Schema.Name : stat.Schema.Label;
                    const std::string detail = hint.empty() ? StatDetail(stat, counting) : hint;
                    const float textX = pos.x + Px(4.0f);
                    drawList->PushClipRect(ImVec2(textX, pos.y), ImVec2(std::max(textX, right), pos.y + rowHeight), true);
                    drawList->AddText(ImVec2(textX, rowTop), ImGui::GetColorU32(ManifoldTheme::Text), label.c_str());
                    drawList->AddText(ImVec2(textX, rowTop + line + Px(2.0f)), ImGui::GetColorU32(hintColor), detail.c_str());
                    drawList->PopClipRect();

                    if (!stat.Settable && anySettable && pointed)
                        ItemTooltip(kind == StatKind::AverageRate ? "The game works this one out by itself." : "Only the game's own server can set this one.");
                }
            }
            clipper.End();
            ImGui::EndChild();

            if (setTo)
            {
                model.SetGameStat(setTo->first, setTo->second);
                g_gameWindow.EditingStat.clear();
            }
        }

        std::string GameStateText(const IdlerModel& model, std::uint32_t appId)
        {
            const RunState state = model.StateOf(appId);
            if (state == RunState::Running)
                return "Idling";
            if (state == RunState::Starting)
                return "Starting";
            const QueueEntry* entry = model.FindEntry(appId);
            if (!entry)
                return "Not in the queue";
            if (entry->Failed)
                return "Did not start";
            if (IsFinished(*entry))
                return "Finished";
            if (entry->Resting && entry->IdledSeconds > 0.0)
                return "On a break";
            return "In the queue";
        }

        void RefreshGameWindowStats(const IdlerModel& model, std::int32_t today)
        {
            const double now = ImGui::GetTime();
            if (now < g_gameWindow.StatsAt)
                return;
            g_gameWindow.StatsAt = now + 1.0;

            const IdleStats& stats = model.Stats();
            const std::uint32_t account = model.Account();
            const std::vector<GameFigures> figures = stats.Figures(account);
            const auto found = std::find_if(figures.begin(), figures.end(), [](const GameFigures& game) { return game.AppId == g_gameWindow.App; });
            g_gameWindow.HasFigures = found != figures.end();
            if (g_gameWindow.HasFigures)
                g_gameWindow.Figures = *found;
            g_gameWindow.AllSeconds = stats.Total(account);
            g_gameWindow.Days = stats.PerDay(account, g_gameWindow.App, today, kChartDays);
        }

        void GameOverview(IdlerModel& model, std::uint32_t appId, const Game* game)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##overview_tab");
            ImGui::PopStyleColor();
            if (ImGui::BeginTable("##overview_columns", 2, ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("Game");
                if (game)
                    GameFactRows(model, *game);
                else
                    InfoRow("App ID", std::to_string(appId), ManifoldTheme::Label);

                ImGui::TableNextColumn();
                ImGui::TextDisabled("Queue");
                if (const QueueEntry* found = model.FindEntry(appId))
                {
                    const QueueEntry entry = *found;
                    QueueEntryRows(model, entry, game, false);
                }
                else
                {
                    Notice("##not_queued", nullptr, ManifoldTheme::Muted, "Not in the queue. Its goal and the rest show here once it is.");
                }
                ImGui::EndTable();
            }
            ImGui::EndChild();
        }

        void GameStatistics(IdlerModel& model, std::uint32_t appId)
        {
            const std::int32_t today = TodayNumber();
            RefreshGameWindowStats(model, today);
            if (!g_gameWindow.HasFigures)
            {
                EmptyState("##none", appId, "Not idled yet.");
                return;
            }
            std::optional<std::int32_t> day;
            GameStatsRows(g_gameWindow.Figures, g_gameWindow.AllSeconds, g_gameWindow.Days, today, ImGui::GetStyle().WindowPadding.y + Px(4.0f),
                [&](std::int32_t clicked) { day = clicked; });
            if (day)
            {
                OpenStatsDay(*day);
                g_page = Page::Statistics;
                model.CloseAchievements();
                ImGui::CloseCurrentPopup();
            }
        }

        void GameWindowHeader(IdlerModel& model, std::uint32_t appId, const Game* game)
        {
            const float height = std::round(Px(112.0f));
            const float width = std::round(height * Covers::kAspect);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float right = start.x + ImGui::GetContentRegionAvail().x;
            CoverPicture(Covers::Get(appId, Covers::Size::Full), ImVec2(width, height));

            const float left = start.x + width + Px(16.0f);
            const float line = ImGui::GetTextLineHeight();
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->PushClipRect(ImVec2(left, start.y), ImVec2(right, start.y + height), true);

            const RunState state = model.StateOf(appId);
            const IdleAnimation::Mood mood = state == RunState::Running ? IdleAnimation::Mood::Idling : state == RunState::Starting ? IdleAnimation::Mood::Starting : IdleAnimation::Mood::Sleeping;
            const float creature = IdleAnimation::Height();
            IdleAnimation::Draw(drawList, ImVec2(left, start.y), mood, appId);
            const std::string creatureName = IdleAnimation::NameOf(appId);
            drawList->AddText(ImVec2(std::round(left + IdleAnimation::Width() + Px(8.0f)), std::round(start.y + (creature - line) * 0.5f)),
                ImGui::GetColorU32(RarityColor(RarityOf(IdleAnimation::CreatureOf(appId)))), creatureName.c_str());

            float y = std::round(start.y + creature + Px(8.0f));
            if (game)
            {
                std::string played = game->PlaytimeMinutes > 0 ? PlaytimeText(game->PlaytimeMinutes) + " played" : std::string("Not played");
                if (game->LastPlayed != 0)
                    played += ", last on " + DateText(game->LastPlayed);
                drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::Label), played.c_str());
                y += line + Px(4.0f);
            }
            std::string where = "Not in the queue";
            ImVec4 whereColor = ManifoldTheme::Muted;
            if (const QueueEntry* entry = model.FindEntry(appId))
                std::tie(where, whereColor) = EntryDetail(*entry, state, model.RestSecondsLeft(appId));
            drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(whereColor), where.c_str());
            drawList->PopClipRect();

            ImGui::SetCursorScreenPos(ImVec2(left, start.y + height - ImGui::GetFrameHeight()));
            BeginRow("##actions");
            GameButtons(model, appId, false);
            EndActionButtons();
            ImGui::PopID();
            ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height + Px(10.0f)));
        }
    }

    void GameWindowDialog(IdlerModel& model)
    {
        if (g_gameWindow.WantsOpen)
        {
            g_gameWindow.WantsOpen = false;
            model.CloseAchievements();
            ImGui::OpenPopup("##game");
            if (g_gameWindow.ReadAtStart)
            {
                g_gameWindow.ReadAtStart = false;
                model.LoadAchievements(g_gameWindow.App);
            }
        }
        if (!ImGui::IsPopupOpen("##game"))
            return;

        const std::uint32_t appId = g_gameWindow.App;
        bool closed = false;
        if (!BeginDialog("##game", ICON_FA_GAMEPAD, GameName(model, appId), Px(760.0f, 560.0f), GameStateText(model, appId), &closed))
            return;
        if (closed)
        {
            model.CloseAchievements();
            ImGui::EndPopup();
            return;
        }

        const Game* found = model.FindGame(appId);
        const std::optional<Game> game = found ? std::optional<Game>(*found) : std::nullopt;

        GameWindowHeader(model, appId, game ? &*game : nullptr);

        const bool achievements = !game || HasAchievements(&*game);
        const AchievementPhase readPhase = model.AchievementsPhase();
        const bool read = readPhase == AchievementPhase::Ready || readPhase == AchievementPhase::Writing;
        const bool stats = achievements || (read && !model.StatList().empty());
        if (!achievements && g_gameWindow.Tab == GameTab::Achievements)
            g_gameWindow.Tab = GameTab::Overview;
        if (!stats && g_gameWindow.Tab == GameTab::GameStats)
            g_gameWindow.Tab = GameTab::Overview;
        const auto tab = [](const char* label, GameTab which)
            {
                if (StatsPill(label, g_gameWindow.Tab == which))
                    g_gameWindow.Tab = which;
            };
        tab(ICON_FA_INFO_CIRCLE "  Overview", GameTab::Overview);
        ImGui::SameLine(0.0f, Px(2.0f));
        tab(ICON_FA_CHART_BAR "  Statistics", GameTab::Statistics);
        if (achievements)
        {
            ImGui::SameLine(0.0f, Px(2.0f));
            std::string label = ICON_FA_TROPHY "  Achievements";
            const AchievementPhase phase = model.AchievementsPhase();
            if (phase == AchievementPhase::Ready || phase == AchievementPhase::Writing)
            {
                const auto& rows = model.AchievementList();
                const auto done = std::count_if(rows.begin(), rows.end(), [](const AchievementRow& row) { return row.Achieved; });
                label += "  " + std::to_string(done) + "/" + std::to_string(rows.size());
            }
            else if (game && game->AchievementsTotal > 0)
            {
                label += "  " + std::to_string(game->AchievementsDone) + "/" + std::to_string(game->AchievementsTotal);
            }
            ImGui::PushID("##achievements_tab");
            tab(label.c_str(), GameTab::Achievements);
            ImGui::PopID();
        }
        if (stats)
        {
            ImGui::SameLine(0.0f, Px(2.0f));
            std::string label = ICON_FA_LIST_OL "  Game stats";
            if (read)
                label += "  " + std::to_string(model.StatList().size());
            ImGui::PushID("##stats_tab");
            tab(label.c_str(), GameTab::GameStats);
            ImGui::PopID();
        }
        ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));

        switch (g_gameWindow.Tab)
        {
        case GameTab::Overview:
            GameOverview(model, appId, game ? &*game : nullptr);
            break;
        case GameTab::Statistics:
            GameStatistics(model, appId);
            break;
        case GameTab::Achievements:
            AchievementsSection(model, appId);
            break;
        case GameTab::GameStats:
            GameStatsSection(model, appId);
            break;
        }
        ImGui::EndPopup();
    }
}
