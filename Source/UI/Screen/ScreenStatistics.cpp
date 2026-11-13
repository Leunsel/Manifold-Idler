#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        enum class StatsTab
        {
            Overview,
            Games,
            History
        };
        StatsTab g_statsTab = StatsTab::Overview;
        std::optional<std::int32_t> g_statsDay;
        enum class StatsChart
        {
            Days30,
            Days90,
            Weekdays,
            Hours
        };
        StatsChart g_statsChart = StatsChart::Days30;
        GameOrder g_statsOrder = GameOrder::Time;
        std::uint32_t g_statsGame = 0;
        double g_statsRefreshAt = -1.0;
        constexpr int kCalendarDays = 378;
        std::vector<double> g_statsCalendar;
        std::vector<double> g_statsGameDays;
        std::vector<GameTotal> g_statsGames;
        std::vector<HistoryRow> g_statsHistory;
        std::vector<GameFigures> g_statsFigures;
        std::vector<GameFigures> g_statsTop;
        StatsSummary g_statsSummary;
        std::array<double, 24> g_statsHours{};
        double g_statsWeek = 0.0;
        double g_statsWeekBefore = 0.0;

        void RefreshStats(const IdlerModel& model, std::int32_t today)
        {
            const double now = ImGui::GetTime();
            if (now < g_statsRefreshAt)
                return;
            g_statsRefreshAt = now + 1.0;

            const IdleStats& stats = model.Stats();
            const std::uint32_t account = model.Account();
            g_statsCalendar = stats.PerDay(account, today, kCalendarDays);
            g_statsWeek = stats.Total(account, today - 6, today);
            g_statsWeekBefore = stats.Total(account, today - 13, today - 7);
            g_statsSummary = stats.Summary(account, today);
            g_statsHours = stats.PerHour(account);
            g_statsGames = stats.ByGame(account, g_statsDay);
            g_statsHistory = stats.History(account, g_statsDay);

            g_statsFigures = stats.Figures(account);
            g_statsTop.assign(g_statsFigures.begin(), g_statsFigures.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(3, g_statsFigures.size())));
            SortFigures(g_statsFigures, g_statsOrder);
            if (std::none_of(g_statsFigures.begin(), g_statsFigures.end(), [](const GameFigures& game) { return game.AppId == g_statsGame; }))
                g_statsGame = g_statsFigures.empty() ? 0u : g_statsFigures.front().AppId;
            g_statsGameDays = stats.PerDay(account, g_statsGame, today, kChartDays);
        }

        std::string SecondsText(double seconds)
        {
            return DurationText(static_cast<std::uint64_t>(seconds));
        }

        std::string ShortSecondsText(double seconds)
        {
            return ShortDurationText(static_cast<std::uint64_t>(seconds));
        }

        std::string RoughSecondsText(double seconds)
        {
            return seconds < 3600.0 ? SecondsText(seconds) : std::to_string(static_cast<std::uint64_t>(std::round(seconds / 3600.0))) + " h";
        }

        void OpenStatsGame(std::uint32_t appId)
        {
            g_statsGame = appId;
            g_statsTab = StatsTab::Games;
            g_statsRefreshAt = -1.0;
        }

        struct CardRowHeight
        {
            float Wanted = 0.0f;
            float Next = 0.0f;
        };
        CardRowHeight g_statsTopRow;
        CardRowHeight g_statsMiddleRow;
        CardRowHeight g_statsLowRow;

        void FillCard(CardRowHeight& row, float top)
        {
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const float used = cursor.y - top;
            row.Next = std::max(row.Next, used);
            ImGui::SetCursorScreenPos(ImVec2(cursor.x, top + std::max(row.Wanted, used)));
            ImGui::Dummy(ImVec2(0.0f, 0.0f));
        }

        void EndCardRow(CardRowHeight& row)
        {
            row.Wanted = std::exchange(row.Next, 0.0f);
        }

        float PillWidth(const char* label)
        {
            return std::round(ImGui::CalcTextSize(label).x + Px(20.0f));
        }

        void GoToTitleLine(float width, float height)
        {
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::SetCursorScreenPos(ImVec2(cursor.x + RowWidth() - width, std::round(cursor.y - ImGui::GetStyle().ItemSpacing.y - (ImGui::GetTextLineHeight() + height) * 0.5f)));
        }

        void StatTile(const char* icon, const char* label, const std::string& value, const std::string& note, const char* tooltip, float width, float height)
        {
            const ImVec2 min = ImGui::GetCursorScreenPos();
            const ImVec2 max(min.x + width, min.y + height);
            ImGui::Dummy(ImVec2(width, height));
            if (tooltip)
                ItemTooltip(tooltip);

            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(min, max, ImGui::GetColorU32(ManifoldTheme::Card), Px(6.0f));
            drawList->AddRect(min, max, ImGui::GetColorU32(ManifoldTheme::CardBorder), Px(6.0f));

            const float left = min.x + Px(11.0f);
            const float right = max.x - Px(8.0f);
            const float line = ImGui::GetTextLineHeight();
            float y = min.y + Px(8.0f);
            drawList->AddText(ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.85f)), icon);
            DrawCutText(drawList, ImVec2(left + std::round(line * 1.45f), y), right, ManifoldTheme::Muted, label);
            y += line + Px(3.0f);
            const float size = std::round(ImGui::GetFontSize() * 1.5f);
            drawList->AddText(ImGui::GetFont(), size, ImVec2(left, y), ImGui::GetColorU32(ManifoldTheme::Text), value.c_str());
            y += size + Px(3.0f);
            DrawCutText(drawList, ImVec2(left, y), right, ManifoldTheme::Muted, note);
        }

        void StatTiles()
        {
            const StatsSummary& summary = g_statsSummary;
            const double today = g_statsCalendar.back();
            const double yesterday = g_statsCalendar[g_statsCalendar.size() - 2];

            std::string trend = g_statsWeek > 0.0 ? "Nothing before" : "";
            if (g_statsWeekBefore > 0.0)
            {
                const auto percent = static_cast<int>(std::round((g_statsWeek / g_statsWeekBefore - 1.0) * 100.0));
                trend = (percent >= 0 ? "+" : "") + std::to_string(percent) + " %";
            }
            static char sessionsNote[80];
            std::snprintf(sessionsNote, sizeof(sessionsNote), "One start of a game. Counted since %s.", summary.FirstSessionDay != 0 ? DayText(summary.FirstSessionDay).c_str() : "the first one");

            constexpr int kTiles = 6;
            const float gap = Px(8.0f);
            const float space = ImGui::GetContentRegionAvail().x;
            const int perRow = (space - gap * (kTiles - 1)) / kTiles >= Px(112.0f) ? kTiles : kTiles / 2;
            const float width = std::floor((space - gap * static_cast<float>(perRow - 1)) / static_cast<float>(perRow));
            const float height = std::round(Px(16.0f) + ImGui::GetTextLineHeight() * 2.0f + Px(6.0f) + ImGui::GetFontSize() * 1.5f);
            int index = 0;
            const auto tile = [&](const char* icon, const char* label, const std::string& value, const std::string& note, const char* tooltip)
                {
                    if (index++ % perRow != 0)
                        ImGui::SameLine(0.0f, gap);
                    StatTile(icon, label, value, note, tooltip, width, height);
                };
            tile(ICON_FA_CALENDAR_DAY, "Today", ShortSecondsText(today), "Yesterday " + RoughSecondsText(yesterday), nullptr);
            tile(ICON_FA_CALENDAR_ALT, "Last 7 days", ShortSecondsText(g_statsWeek), trend, "Against the 7 days before.");
            tile(ICON_FA_HOURGLASS_HALF, "All time", ShortSecondsText(summary.Seconds), summary.DaysIdled > 0 ? "Since " + DayText(summary.FirstDay) : std::string("Nothing yet"), nullptr);
            tile(ICON_FA_GAMEPAD, "Games", std::to_string(summary.Games), "On " + std::to_string(summary.DaysIdled) + (summary.DaysIdled == 1 ? " day" : " days"), nullptr);
            tile(ICON_FA_REDO, "Sessions", std::to_string(summary.Sessions), summary.LongestSession > 0.0 ? "Longest " + RoughSecondsText(summary.LongestSession) : std::string("None yet"), sessionsNote);
            tile(ICON_FA_FIRE, "Days in a row", std::to_string(summary.CurrentStreak), "Best " + std::to_string(summary.LongestStreak), nullptr);
        }

        double ChartTop(double seconds)
        {
            const double hours = seconds / 3600.0;
            double top = 0.5;
            for (const double step : { 0.5, 1.0, 2.0, 4.0, 10.0, 20.0, 40.0, 100.0, 200.0, 400.0, 1000.0, 2000.0, 4000.0, 10000.0 })
            {
                top = step;
                if (step >= hours)
                    break;
            }
            return top;
        }

        std::string AxisText(double hours)
        {
            return hours < 1.0 ? std::to_string(static_cast<int>(std::round(hours * 60.0))) + " min" : HoursText(static_cast<float>(hours));
        }

        struct BarChartInput
        {
            const std::vector<double>& Seconds;
            float Height = 0.0f;
            std::function<std::string(int)> Label{};
            std::function<void(int)> Tooltip{};
            std::function<void(int)> Clicked{};
            int Selected = -1;
            const std::vector<double>* Line = nullptr;
            const char* Empty = "Nothing idled yet.";
        };

        void BarChart(const char* id, const BarChartInput& chart)
        {
            BeginRow(id);
            const float width = RowWidth();
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 plotMin(pos.x + Px(40.0f), pos.y + Px(4.0f));
            const ImVec2 plotMax(pos.x + width, pos.y + chart.Height - line - Px(5.0f));
            const float plotHeight = plotMax.y - plotMin.y;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const int count = static_cast<int>(chart.Seconds.size());
            const double most = count > 0 ? *std::max_element(chart.Seconds.begin(), chart.Seconds.end()) : 0.0;
            const double top = ChartTop(most);
            for (const double part : { 0.0, 0.5, 1.0 })
            {
                const float y = std::round(plotMax.y - plotHeight * static_cast<float>(part));
                drawList->AddLine(ImVec2(plotMin.x, y), ImVec2(plotMax.x, y), ImGui::GetColorU32(ManifoldTheme::RowSeparator));
                const std::string label = part == 0.0 ? std::string("0") : AxisText(top * part);
                const ImVec2 size = ImGui::CalcTextSize(label.c_str());
                drawList->AddText(ImVec2(plotMin.x - size.x - Px(6.0f), y - size.y * 0.5f), ImGui::GetColorU32(ManifoldTheme::Muted), label.c_str());
            }

            const float slot = (plotMax.x - plotMin.x) / static_cast<float>(std::max(count, 1));
            const float barWidth = std::max(1.0f, std::round(std::min(slot - Px(2.0f), Px(24.0f))));
            const float rounding = std::min(Px(4.0f), barWidth * 0.5f);
            const float toY = plotHeight / static_cast<float>(top * 3600.0);
            int hoveredBar = -1;
            for (int index = 0; index < count; ++index)
            {
                const double seconds = chart.Seconds[static_cast<std::size_t>(index)];
                const float x = plotMin.x + slot * static_cast<float>(index);

                ImGui::SetCursorScreenPos(ImVec2(x, plotMin.y));
                ImGui::PushID(index);
                const bool clicked = ImGui::InvisibleButton("##bar", ImVec2(std::max(slot, 1.0f), std::max(plotHeight, 1.0f)));
                ImGui::PopID();
                if (clicked && chart.Clicked)
                    chart.Clicked(index);
                const bool hovered = ImGui::IsItemHovered();
                if (hovered)
                {
                    hoveredBar = index;
                    drawList->AddRectFilled(ImVec2(x, plotMin.y), ImVec2(x + slot, plotMax.y), IM_COL32(255, 255, 255, 9));
                }

                if (seconds > 0.0)
                {
                    const float barHeight = std::max(Px(2.0f), static_cast<float>(seconds) * toY);
                    const float left = std::round(x + (slot - barWidth) * 0.5f);
                    const bool dimmed = chart.Selected >= 0 && chart.Selected != index;
                    const ImVec4 color = hovered ? ManifoldTheme::AccentHovered : dimmed ? ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.4f) : ManifoldTheme::Accent;
                    drawList->AddRectFilled(ImVec2(left, plotMax.y - barHeight), ImVec2(left + barWidth, plotMax.y), ImGui::GetColorU32(color), std::min(rounding, barHeight), ImDrawFlags_RoundCornersTop);
                }

                const std::string label = chart.Label ? chart.Label(index) : std::string();
                if (!label.empty())
                {
                    const ImVec2 size = ImGui::CalcTextSize(label.c_str());
                    const float labelX = std::clamp(x + (slot - size.x) * 0.5f, plotMin.x, std::max(plotMin.x, plotMax.x - size.x));
                    drawList->AddText(ImVec2(labelX, plotMax.y + Px(4.0f)), ImGui::GetColorU32(ManifoldTheme::Muted), label.c_str());
                }
            }

            if (chart.Line && static_cast<int>(chart.Line->size()) == count && count > 1 && most > 0.0)
            {
                std::vector<ImVec2> points;
                points.reserve(chart.Line->size());
                for (int index = 0; index < count; ++index)
                    points.emplace_back(plotMin.x + slot * (static_cast<float>(index) + 0.5f), plotMax.y - static_cast<float>((*chart.Line)[static_cast<std::size_t>(index)]) * toY);
                drawList->AddPolyline(points.data(), count, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.75f)), ImDrawFlags_None, std::max(1.0f, Px(1.5f)));
            }

            if (hoveredBar >= 0 && chart.Tooltip)
                ShowTooltip([&]() { chart.Tooltip(hoveredBar); });

            if (most <= 0.0 && chart.Empty)
            {
                const ImVec2 size = ImGui::CalcTextSize(chart.Empty);
                drawList->AddText(ImVec2((plotMin.x + plotMax.x - size.x) * 0.5f, (plotMin.y + plotMax.y - size.y) * 0.5f), ImGui::GetColorU32(ManifoldTheme::Muted), chart.Empty);
            }

            ImGui::SetCursorScreenPos(pos);
            ImGui::Dummy(ImVec2(width, chart.Height));
            ImGui::PopID();
        }

        std::vector<double> WeekdaySeconds()
        {
            return { g_statsSummary.Weekdays.begin(), g_statsSummary.Weekdays.end() };
        }

        void WeekdayChart(const char* id, float height)
        {
            static constexpr const char* kDays[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
            const std::vector<double> seconds = WeekdaySeconds();
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Label = [](int index) { return std::string(kDays[index]); };
            chart.Tooltip = [&](int index)
                {
                    const double value = seconds[static_cast<std::size_t>(index)];
                    ImGui::TextUnformatted(SecondsText(value).c_str());
                    ImGui::TextDisabled("%s, %d %% of all", kDays[index], g_statsSummary.Seconds > 0.0 ? static_cast<int>(std::round(value / g_statsSummary.Seconds * 100.0)) : 0);
                };
            BarChart(id, chart);
        }

        void HourChart(const char* id, float height)
        {
            const std::vector<double> seconds(g_statsHours.begin(), g_statsHours.end());
            const auto hourText = [](int hour)
                {
                    char text[8];
                    std::snprintf(text, sizeof(text), "%02d:00", hour);
                    return std::string(text);
                };
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Empty = "Fills up while games idle.";
            chart.Label = [&](int index) { return index % 6 == 0 ? hourText(index) : std::string(); };
            chart.Tooltip = [&](int index)
                {
                    ImGui::TextUnformatted(SecondsText(seconds[static_cast<std::size_t>(index)]).c_str());
                    ImGui::TextDisabled("%s to %s", hourText(index).c_str(), hourText((index + 1) % 24).c_str());
                };
            BarChart(id, chart);
        }

        void DaysChart(std::int32_t today, float height, int days)
        {
            const std::size_t first = g_statsCalendar.size() - static_cast<std::size_t>(days);
            const std::vector<double> seconds(g_statsCalendar.begin() + static_cast<std::ptrdiff_t>(first), g_statsCalendar.end());
            std::vector<double> average(seconds.size(), 0.0);
            for (std::size_t index = 0; index < average.size(); ++index)
            {
                double sum = 0.0;
                for (std::size_t back = 0; back < 7; ++back)
                    sum += g_statsCalendar[first + index - back];
                average[index] = sum / 7.0;
            }

            const std::int32_t firstDay = today - days + 1;
            BarChartInput chart{ .Seconds = seconds };
            chart.Height = height;
            chart.Line = &average;
            chart.Label = [=](int index) -> std::string
                {
                    if ((days - 1 - index) % (days > 30 ? 21 : 7) != 0)
                        return {};
                    const std::int32_t day = firstDay + index;
                    return day == today ? std::string("Today") : DayText(day).substr(5);
                };
            chart.Tooltip = [&](int index)
                {
                    ImGui::TextUnformatted(SecondsText(seconds[static_cast<std::size_t>(index)]).c_str());
                    ImGui::TextDisabled("%s", DayText(firstDay + index).c_str());
                    ImGui::TextDisabled("7-day average %s", SecondsText(average[static_cast<std::size_t>(index)]).c_str());
                };
            chart.Clicked = [=](int index) { OpenStatsDay(firstDay + index); };
            if (g_statsDay && *g_statsDay >= firstDay && *g_statsDay <= today)
                chart.Selected = *g_statsDay - firstDay;
            BarChart("##days", chart);
        }

        void HoursCard(std::int32_t today, float height, bool withTimes)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Hours idled");
            if (!withTimes && (g_statsChart == StatsChart::Weekdays || g_statsChart == StatsChart::Hours))
                g_statsChart = StatsChart::Days30;

            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                struct Choice
                {
                    const char* Label;
                    StatsChart Chart;
                };
                static constexpr Choice kChoices[] = { { "30 days", StatsChart::Days30 }, { "90 days", StatsChart::Days90 }, { "Weekdays", StatsChart::Weekdays }, { "Hours", StatsChart::Hours } };
                const int shown = withTimes ? 4 : 2;
                const float gap = Px(2.0f);
                float pills = -gap;
                for (int index = 0; index < shown; ++index)
                    pills += PillWidth(kChoices[index].Label) + gap;
                const float button = ImGui::GetFrameHeight();
                const float room = RowWidth();
                GoToTitleLine(pills, button);
                const ImVec2 at = ImGui::GetCursorScreenPos();
                for (int index = 0; index < shown; ++index)
                {
                    if (index > 0)
                        ImGui::SameLine(0.0f, gap);
                    if (StatsPill(kChoices[index].Label, g_statsChart == kChoices[index].Chart))
                        g_statsChart = kChoices[index].Chart;
                }

                constexpr const char* kLegend = "7-day average";
                const ImVec2 size = ImGui::CalcTextSize(kLegend);
                const bool days = g_statsChart == StatsChart::Days30 || g_statsChart == StatsChart::Days90;
                if (days && room - pills - ImGui::CalcTextSize("Hours idled").x > size.x + Px(48.0f))
                {
                    const float textX = at.x - size.x - Px(12.0f);
                    const float y = std::round(at.y + (button - size.y) * 0.5f);
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    drawList->AddText(ImVec2(textX, y), ImGui::GetColorU32(ManifoldTheme::Muted), kLegend);
                    const float lineY = std::round(y + size.y * 0.5f);
                    drawList->AddLine(ImVec2(textX - Px(20.0f), lineY), ImVec2(textX - Px(5.0f), lineY), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, 0.75f)), std::max(1.0f, Px(1.5f)));
                }
                ImGui::SetCursorScreenPos(cursor);
            }

            switch (g_statsChart)
            {
            case StatsChart::Weekdays: WeekdayChart("##weekdays", height); break;
            case StatsChart::Hours: HourChart("##hours", height); break;
            case StatsChart::Days90: DaysChart(today, height, 90); break;
            default: DaysChart(today, height, 30); break;
            }
            FillCard(g_statsTopRow, top);
            EndCard();
        }

        void PodiumCard(const IdlerModel& model, float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Most idled");
            if (g_statsTop.empty())
            {
                EmptyState("##none", 105u, "Nothing idled yet.");
            }
            else
            {
                BeginRow("##podium");
                const float width = RowWidth();
                const ImVec2 pos = ImGui::GetCursorScreenPos();
                const float line = ImGui::GetTextLineHeight();
                const float pixel = std::clamp(std::round(height / 62.0f), std::round(Px(3.0f)), std::round(Px(6.0f)));
                const ImVec2 picture = IdleAnimation::SizeAt(pixel);
                const float labels = line * 2.0f + Px(8.0f);
                const float highest = std::max(Px(30.0f), height - picture.y - labels);
                const float column = std::floor(width / 3.0f);
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                static constexpr int kPlaces[3] = { 1, 0, 2 };
                static constexpr float kSteps[3] = { 1.0f, 0.64f, 0.36f };
                static constexpr float kFill[3] = { 0.46f, 0.28f, 0.18f };
                const float ground = pos.y + picture.y + highest;

                const auto centred = [&](float left, float y, ImVec4 color, const std::string& text)
                    {
                        const float room = column - Px(6.0f);
                        const float size = ImGui::CalcTextSize(text.c_str()).x;
                        DrawCutText(drawList, ImVec2(std::round(left + Px(3.0f) + std::max(0.0f, (room - size) * 0.5f)), y), left + column - Px(3.0f), color, text);
                    };

                for (std::size_t rank = 0; rank < g_statsTop.size(); ++rank)
                {
                    const GameFigures& game = g_statsTop[rank];
                    const float left = pos.x + column * static_cast<float>(kPlaces[rank]);
                    const std::string name = GameName(model, game.AppId);
                    ImGui::SetCursorScreenPos(ImVec2(left, pos.y));
                    ImGui::PushID(static_cast<int>(rank));
                    const bool clicked = ImGui::InvisibleButton("##place", ImVec2(column, height));
                    ImGui::PopID();
                    const bool hovered = ImGui::IsItemHovered();
                    if (hovered)
                    {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ShowTooltip([&]()
                            {
                                ImGui::TextUnformatted(name.c_str());
                                ImGui::TextDisabled("%s", SecondsText(game.Seconds).c_str());
                            });
                    }
                    if (clicked)
                        OpenStatsGame(game.AppId);

                    const float step = std::round(highest * kSteps[rank]);
                    const float inset = Px(5.0f);
                    drawList->AddRectFilled(ImVec2(left + inset, ground - step), ImVec2(left + column - inset, ground),
                        ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, hovered ? kFill[rank] + 0.12f : kFill[rank])), Px(4.0f), ImDrawFlags_RoundCornersTop);
                    const std::string place = std::to_string(rank + 1);
                    const float size = std::round(ImGui::GetFontSize() * (rank == 0 ? 1.7f : 1.35f));
                    drawList->AddText(ImGui::GetFont(), size, ImVec2(std::round(left + (column - ImGui::CalcTextSize(place.c_str()).x * size / ImGui::GetFontSize()) * 0.5f), ground - step + Px(5.0f)),
                        ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Text, rank == 0 ? 0.95f : 0.75f)), place.c_str());

                    const IdleAnimation::Look look = IsShiny(game.AppId) ? IdleAnimation::Look::Shiny : IdleAnimation::Look::Normal;
                    IdleAnimation::DrawCreature(drawList, ImVec2(std::round(left + (column - picture.x) * 0.5f), ground - step - picture.y), IdleAnimation::CreatureOf(game.AppId), IdleAnimation::Mood::Idling, game.AppId, pixel, look);

                    centred(left, ground + Px(5.0f), ManifoldTheme::Label, name);
                    centred(left, ground + Px(5.0f) + line + Px(2.0f), ManifoldTheme::Muted, ShortSecondsText(game.Seconds));
                }
                IdleAnimation::RequestFrames();
                ImGui::SetCursorScreenPos(pos);
                ImGui::Dummy(ImVec2(width, height));
                ImGui::PopID();
            }
            FillCard(g_statsTopRow, top);
            EndCard();
        }

        void CalendarCard(std::int32_t today)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Calendar");
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const auto cellColor = [](double share) -> ImU32
                {
                    if (share <= 0.0)
                        return IM_COL32(255, 255, 255, 9);
                    const float level = std::ceil(static_cast<float>(std::sqrt(share)) * 4.0f);
                    static constexpr float kAlpha[4] = { 0.26f, 0.46f, 0.72f, 1.0f };
                    return ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, kAlpha[std::clamp(static_cast<int>(level) - 1, 0, 3)]));
                };

            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float square = std::round(Px(9.0f));
                const float gap = std::max(1.0f, std::round(Px(2.0f)));
                const ImVec2 less = ImGui::CalcTextSize("Less");
                const ImVec2 more = ImGui::CalcTextSize("More");
                const float legend = less.x + more.x + Px(12.0f) + square * 4.0f + gap * 3.0f;
                GoToTitleLine(legend, ImGui::GetTextLineHeight());
                const ImVec2 at = ImGui::GetCursorScreenPos();
                drawList->AddText(at, ImGui::GetColorU32(ManifoldTheme::Muted), "Less");
                float x = at.x + less.x + Px(6.0f);
                const float y = std::round(at.y + (less.y - square) * 0.5f);
                for (const double share : { 0.05, 0.2, 0.5, 1.0 })
                {
                    drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + square, y + square), cellColor(share), Px(2.0f));
                    x += square + gap;
                }
                drawList->AddText(ImVec2(x + Px(6.0f) - gap, at.y), ImGui::GetColorU32(ManifoldTheme::Muted), "More");
                ImGui::SetCursorScreenPos(cursor);
            }

            BeginRow("##calendar");
            const float width = RowWidth();
            const float labels = Px(30.0f);
            const float gap = std::max(1.0f, std::round(Px(2.0f)));
            const float cell = std::clamp(std::floor((width - labels) / 26.0f - gap), Px(8.0f), Px(16.0f));
            const int weeks = std::clamp(static_cast<int>((width - labels + gap) / (cell + gap)), 1, 53);
            const float line = ImGui::GetTextLineHeight();
            const ImVec2 pos = ImGui::GetCursorScreenPos();
            const ImVec2 grid(pos.x + labels, pos.y + line + Px(3.0f));
            const float height = line + Px(3.0f) + (cell + gap) * 7.0f - gap;

            const bool clicked = ImGui::InvisibleButton("##days", ImVec2(width, height));
            const bool hovered = ImGui::IsItemHovered();
            const ImVec2 mouse = ImGui::GetMousePos();
            std::int32_t underMouse = 0;
            bool found = false;

            const double best = *std::max_element(g_statsCalendar.begin(), g_statsCalendar.end());
            const std::int32_t lastMonday = today - WeekdayOf(today);
            static constexpr const char* kMonths[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
            int previousMonth = -1;
            int labelledAt = -100;
            for (int week = 0; week < weeks; ++week)
            {
                const std::int32_t monday = lastMonday - 7 * (weeks - 1 - week);
                const float x = grid.x + (cell + gap) * static_cast<float>(week);

                const std::string date = DayText(monday);
                const int month = (date[5] - '0') * 10 + (date[6] - '0') - 1;
                if (month != previousMonth)
                {
                    if (week - labelledAt >= 3 && month >= 0 && month < 12 && (previousMonth >= 0 || week == 0))
                    {
                        drawList->AddText(ImVec2(x, pos.y), ImGui::GetColorU32(ManifoldTheme::Muted), kMonths[month]);
                        labelledAt = week;
                    }
                    previousMonth = month;
                }

                for (int row = 0; row < 7; ++row)
                {
                    const std::int32_t day = monday + row;
                    if (day > today)
                        continue;
                    const double seconds = g_statsCalendar[g_statsCalendar.size() - 1 - static_cast<std::size_t>(today - day)];
                    const ImVec2 min(x, grid.y + (cell + gap) * static_cast<float>(row));
                    const ImVec2 max(min.x + cell, min.y + cell);
                    drawList->AddRectFilled(min, max, cellColor(best > 0.0 ? seconds / best : 0.0), Px(2.0f));
                    const bool under = hovered && mouse.x >= min.x && mouse.x < max.x + gap && mouse.y >= min.y && mouse.y < max.y + gap;
                    if (under)
                    {
                        underMouse = day;
                        found = true;
                    }
                    if (under || (g_statsDay && *g_statsDay == day))
                        drawList->AddRect(min, max, ImGui::GetColorU32(under ? ManifoldTheme::Text : ManifoldTheme::Label), Px(2.0f));
                }
            }
            for (const int row : { 0, 2, 4 })
            {
                static constexpr const char* kDays[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
                drawList->AddText(ImVec2(pos.x, std::round(grid.y + (cell + gap) * static_cast<float>(row) + (cell - line) * 0.5f)), ImGui::GetColorU32(ManifoldTheme::Muted), kDays[row]);
            }
            if (found)
            {
                const double seconds = g_statsCalendar[g_statsCalendar.size() - 1 - static_cast<std::size_t>(today - underMouse)];
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                ShowTooltip([&]()
                    {
                        ImGui::TextUnformatted(seconds > 0.0 ? SecondsText(seconds).c_str() : "Nothing idled");
                        ImGui::TextDisabled("%s", DayText(underMouse).c_str());
                    });
                if (clicked)
                    OpenStatsDay(underMouse);
            }
            ImGui::PopID();
            FillCard(g_statsMiddleRow, top);
            EndCard();
        }

        void RecordsCard(const IdlerModel& model)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("Records");
            const StatsSummary& summary = g_statsSummary;
            if (summary.DaysIdled == 0)
            {
                EmptyState("##none", 106u, "Nothing idled yet.");
            }
            else
            {
                InfoRow("Best day", ShortSecondsText(summary.BestDaySeconds) + ", " + DayText(summary.BestDay), ManifoldTheme::Label);
                InfoRow("Most games", std::to_string(summary.MostGamesInADay) + " on " + DayText(summary.MostGamesDay), ManifoldTheme::Label);
                InfoRow("Most in a row", std::to_string(summary.LongestStreak) + (summary.LongestStreak == 1 ? " day" : " days"), ManifoldTheme::Label);
                InfoRow("Average day", ShortSecondsText(summary.Seconds / static_cast<double>(summary.DaysIdled)), ManifoldTheme::Label);
                if (summary.LongestSession > 0.0)
                    InfoRow("Longest run", ShortSecondsText(summary.LongestSession) + ", " + GameName(model, summary.LongestSessionApp), ManifoldTheme::Label);
                if (summary.MostSessions > 0)
                    InfoRow("Most sessions", std::to_string(summary.MostSessions) + ", " + GameName(model, summary.MostSessionsApp), ManifoldTheme::Label);
            }
            FillCard(g_statsMiddleRow, top);
            EndCard();
        }

        void WeekdayCard(float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("By day of the week");
            WeekdayChart("##weekdays", height);
            FillCard(g_statsLowRow, top);
            EndCard();
        }

        void HourCard(float height)
        {
            const float top = ImGui::GetCursorScreenPos().y;
            BeginCard("By hour of the day");
            HourChart("##hours", height);
            FillCard(g_statsLowRow, top);
            EndCard();
        }

        float g_statsTilesUsed = 80.0f;
        float g_statsTopChrome = 70.0f;
        float g_statsMiddleUsed = 200.0f;
        float g_statsLowChrome = 60.0f;

        void StatsOverview(const IdlerModel& model, std::int32_t today)
        {
            const float page = ImGui::GetContentRegionAvail().y;
            const float spare = page - g_statsTilesUsed - g_statsTopChrome - g_statsMiddleUsed - Px(2.0f);
            const bool threeRows = spare - g_statsLowChrome >= Px(130.0f) + Px(96.0f);
            const float free = threeRows ? spare - g_statsLowChrome : spare;
            const float tall = std::max(Px(100.0f), threeRows ? std::round(free * 0.62f) : free);
            const float low = std::max(Px(80.0f), free - tall);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::BeginChild("##overview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, spare >= Px(100.0f) ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None);
            ImGui::PopStyleColor();
            SetScrollSurface(ManifoldTheme::Page);

            const float start = ImGui::GetCursorScreenPos().y;
            StatTiles();
            const float afterTiles = ImGui::GetCursorScreenPos().y;

            const float closer = Px(18.0f);
            CardColumns("##top", [&]() { HoursCard(today, tall, !threeRows); }, [&]() { PodiumCard(model, tall); }, 0.66f);
            EndCardRow(g_statsTopRow);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - closer);
            const float afterTop = ImGui::GetCursorScreenPos().y;

            CardColumns("##middle", [&]() { CalendarCard(today); }, [&]() { RecordsCard(model); }, 0.66f);
            EndCardRow(g_statsMiddleRow);
            const bool lastRowIsMiddle = !threeRows;
            if (!lastRowIsMiddle)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - closer);
            const float afterMiddle = ImGui::GetCursorScreenPos().y;
            g_statsMiddleUsed = afterMiddle - afterTop + (lastRowIsMiddle ? -closer : 0.0f);

            if (threeRows)
            {
                CardColumns("##low", [&]() { WeekdayCard(low); }, [&]() { HourCard(low); }, 0.5f);
                EndCardRow(g_statsLowRow);
                g_statsLowChrome = ImGui::GetCursorScreenPos().y - afterMiddle - low - closer;
            }
            g_statsTilesUsed = afterTiles - start;
            g_statsTopChrome = afterTop - afterTiles - tall;
            ImGui::EndChild();
        }

        void StatsGameList(const IdlerModel& model)
        {
            BeginCard("Games");
            {
                const ImVec2 cursor = ImGui::GetCursorScreenPos();
                const float width = Px(136.0f);
                GoToTitleLine(width, ImGui::GetFrameHeight());
                if (ChoiceCombo("##order", g_statsOrder, GameOrder::Count, GameOrderName, width))
                    SortFigures(g_statsFigures, g_statsOrder);
                ImGui::SetCursorScreenPos(cursor);
            }

            const auto measure = [](const GameFigures& game) -> double
                {
                    switch (g_statsOrder)
                    {
                    case GameOrder::Sessions: return static_cast<double>(game.Sessions);
                    case GameOrder::LongestSession: return game.LongestSession;
                    case GameOrder::Days: return static_cast<double>(game.Days);
                    default: return game.Seconds;
                    }
                };
            double most = 0.0;
            for (const GameFigures& game : g_statsFigures)
                most = std::max(most, measure(game));

            BeginCardList("##games", 0.0f);
            if (g_statsFigures.empty())
                EmptyState("##none", 105u, "Nothing idled yet.");
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsFigures.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const GameFigures& game = g_statsFigures[static_cast<std::size_t>(row)];
                    const std::string time = SecondsText(game.Seconds);
                    const std::string sessions = std::to_string(game.Sessions) + (game.Sessions == 1 ? " session" : " sessions");
                    std::string detail = time;
                    switch (g_statsOrder)
                    {
                    case GameOrder::Sessions: detail = sessions + ", " + time; break;
                    case GameOrder::LongestSession: detail = "Longest " + SecondsText(game.LongestSession) + ", " + time; break;
                    case GameOrder::Days: detail = std::to_string(game.Days) + (game.Days == 1 ? " day, " : " days, ") + time; break;
                    case GameOrder::Recent: detail = DayText(game.LastDay) + ", " + time; break;
                    default:
                        if (game.Sessions > 0)
                            detail = time + ", " + sessions;
                        break;
                    }
                    GameRowOptions options;
                    if (g_statsOrder != GameOrder::Recent && most > 0.0)
                        options.Progress = static_cast<float>(measure(game) / most);
                    const bool selected = game.AppId == g_statsGame;
                    ImGui::PushID(static_cast<int>(game.AppId));
                    if (GameRow("##game", selected, Covers::Get(game.AppId, Covers::Size::Thumb), GameName(model, game.AppId), selected ? ManifoldTheme::Text : ManifoldTheme::Label,
                        detail, ManifoldTheme::Muted, {}, options))
                    {
                        g_statsGame = game.AppId;
                        g_statsRefreshAt = -1.0;
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        OpenGameWindow(game.AppId, GameTab::Statistics);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void StatsGameCard(const IdlerModel& model, std::int32_t today)
        {
            BeginCard("Selected game");
            const auto found = std::find_if(g_statsFigures.begin(), g_statsFigures.end(), [](const GameFigures& game) { return game.AppId == g_statsGame; });
            if (found == g_statsFigures.end())
            {
                EmptyState("##none", 103u, "Pick a game in the list.");
                EndCard();
                return;
            }

            const GameFigures game = *found;
            CoverRow("##cover", Covers::Get(game.AppId, Covers::Size::Full), Px(84.0f));
            InfoRow("Name", GameName(model, game.AppId));
            GameStatsRows(game, g_statsSummary.Seconds, g_statsGameDays, today, ImGui::GetStyle().ItemSpacing.y * 2.0f + Px(4.0f), [](std::int32_t day) { OpenStatsDay(day); });
            EndCard();
        }

        void ByGameCard(const IdlerModel& model)
        {
            BeginCard("By game");
            BeginCardList("##by_game", 0.0f);
            if (g_statsGames.empty())
                EmptyState("##none", 105u, g_statsDay ? "Nothing idled on this day." : "Nothing idled yet.");

            const double most = g_statsGames.empty() ? 1.0 : g_statsGames.front().Seconds;
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsGames.size()), GameRowHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const GameTotal& total = g_statsGames[static_cast<std::size_t>(row)];
                    GameRowOptions options;
                    options.Progress = static_cast<float>(total.Seconds / most);
                    ImGui::PushID(static_cast<int>(total.AppId));
                    GameRow("##game", false, Covers::Get(total.AppId, Covers::Size::Thumb), GameName(model, total.AppId), ManifoldTheme::Label,
                        SecondsText(total.Seconds), ManifoldTheme::Muted, {}, options);
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void HistoryCard(const IdlerModel& model)
        {
            BeginCard("History");
            BeginCardList("##history", 0.0f);
            if (g_statsHistory.empty())
                EmptyState("##none", 106u, g_statsDay ? "Nothing idled on this day." : "Nothing idled yet.");

            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(g_statsHistory.size()), ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y);
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    const HistoryRow& entry = g_statsHistory[static_cast<std::size_t>(row)];
                    ImGui::PushID(row);
                    ListRow("##row", false, ICON_FA_CLOCK, ManifoldTheme::Muted, GameName(model, entry.AppId), ManifoldTheme::Label,
                        { { SecondsText(entry.Seconds), ManifoldTheme::Label }, { DayText(entry.Day), ManifoldTheme::Muted } });
                    ImGui::PopID();
                }
            }
            clipper.End();
            EndCardList();
            EndCard();
        }

        void StatsTabs()
        {
            const auto tab = [](const char* label, StatsTab which)
                {
                    if (StatsPill(label, g_statsTab == which))
                        g_statsTab = which;
                };
            tab(ICON_FA_CHART_BAR "  Overview", StatsTab::Overview);
            ImGui::SameLine(0.0f, Px(2.0f));
            tab(ICON_FA_GAMEPAD "  Games", StatsTab::Games);
            ImGui::SameLine(0.0f, Px(2.0f));
            tab(ICON_FA_HISTORY "  History", StatsTab::History);

            if (g_statsTab == StatsTab::History && g_statsDay)
            {
                ImGui::SameLine(0.0f, Px(12.0f));
                const std::string label = std::string(ICON_FA_TIMES "  ") + DayText(*g_statsDay);
                if (ImGui::Button(label.c_str()))
                {
                    g_statsDay.reset();
                    g_statsRefreshAt = -1.0;
                }
                ItemTooltip("Back to all days.");
            }
        }
    }

    void OpenStatsDay(std::int32_t day)
    {
        g_statsDay = day;
        g_statsTab = StatsTab::History;
        g_statsRefreshAt = -1.0;
    }

    bool StatsPill(const char* label, bool active)
    {
        const ImVec2 text = ImGui::CalcTextSize(label);
        const ImVec2 size(std::round(text.x + Px(20.0f)), ImGui::GetFrameHeight());
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton(label, size);
        const bool hovered = ImGui::IsItemHovered();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (active || hovered)
            drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), active ? ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.16f)) : IM_COL32(255, 255, 255, 12), Px(5.0f));
        drawList->AddText(ImVec2(std::round(min.x + (size.x - text.x) * 0.5f), std::round(min.y + (size.y - text.y) * 0.5f)),
            ImGui::GetColorU32(active ? ManifoldTheme::Accent : hovered ? ManifoldTheme::Text : ManifoldTheme::Label), label);
        return clicked;
    }

    void GameStatsRows(const GameFigures& game, double allSeconds, const std::vector<double>& days, std::int32_t today, float below,
        const std::function<void(std::int32_t)>& clickedDay)
    {
        InfoRow("Idled", SecondsText(game.Seconds), ManifoldTheme::Label);
        const double share = allSeconds > 0.0 ? game.Seconds / allSeconds * 100.0 : 0.0;
        InfoRow("Of all time", std::to_string(static_cast<int>(std::round(share))) + " %", ManifoldTheme::Label);
        if (game.Sessions > 0)
            InfoRow("Sessions", std::to_string(game.Sessions) + ", longest " + SecondsText(game.LongestSession), ManifoldTheme::Label);
        InfoRow("Days", std::to_string(game.Days) + ", " + DayText(game.FirstDay) + " to " + DayText(game.LastDay), ManifoldTheme::Label);

        BeginRow("##chart_title");
        ImGui::TextDisabled("Last 30 days");
        ImGui::PopID();
        const std::int32_t firstDay = today - kChartDays + 1;
        BarChartInput chart{ .Seconds = days };
        chart.Height = std::clamp(ImGui::GetContentRegionAvail().y - below, Px(84.0f), Px(300.0f));
        chart.Empty = "Not in the last 30 days.";
        chart.Label = [=](int index) -> std::string
            {
                if ((kChartDays - 1 - index) % 7 != 0)
                    return {};
                const std::int32_t day = firstDay + index;
                return day == today ? std::string("Today") : DayText(day).substr(5);
            };
        chart.Tooltip = [&](int index)
            {
                ImGui::TextUnformatted(SecondsText(days[static_cast<std::size_t>(index)]).c_str());
                ImGui::TextDisabled("%s", DayText(firstDay + index).c_str());
            };
        if (clickedDay)
            chart.Clicked = [=](int index) { clickedDay(firstDay + index); };
        BarChart("##game_days", chart);
    }

    void StatisticsPage(Shell& app)
    {
        IdlerModel& model = app.Model();
        const std::int32_t today = TodayNumber();
        RefreshStats(model, today);

        BeginPage(app, g_statsTab == StatsTab::History && g_statsDay ? DayText(*g_statsDay) : std::string("idled by this program"));
        StatsTabs();
        switch (g_statsTab)
        {
        case StatsTab::Games:
            CardColumns("##stats_games",
                [&]() { StatsGameList(model); },
                [&]() { StatsGameCard(model, today); }, 0.58f, Px(kDetailColumnWidth));
            break;
        case StatsTab::History:
            CardColumns("##stats_columns",
                [&]() { ByGameCard(model); },
                [&]() { HistoryCard(model); });
            break;
        default:
            StatsOverview(model, today);
            break;
        }
        EndPageBody();
    }
}
