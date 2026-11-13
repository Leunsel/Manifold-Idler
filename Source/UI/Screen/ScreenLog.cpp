#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        std::vector<LogEntry> g_logEntries;
        std::vector<std::size_t> g_logShown;
        std::uint64_t g_logGeneration = ~0ull;
        std::uint64_t g_logScrolled = ~0ull;
        bool g_logAutoScroll = true;
        bool g_logFilterChanged = true;
        LogLevel g_logLowest = LogLevel::Info;
        char g_logSearch[64] = "";

        struct LogLevelChoice
        {
            LogLevel Lowest;
            const char* Label;
        };

        constexpr LogLevelChoice kLogLevels[] = {
            { LogLevel::Info, "All entries" },
            { LogLevel::Warning, "Warnings and errors" },
            { LogLevel::Error, "Errors only" },
        };

        ImVec4 LogColor(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Success: return ImVec4(0.34f, 1.0f, 0.52f, 1.0f);
            case LogLevel::Warning: return ImVec4(1.0f, 0.82f, 0.28f, 1.0f);
            case LogLevel::Error: return ImVec4(1.0f, 0.34f, 0.34f, 1.0f);
            default: return ImVec4(0.34f, 0.94f, 1.0f, 1.0f);
            }
        }
    }

    void LogPage(Shell& app)
    {
        const std::uint64_t generation = RuntimeLog::Generation();
        if (g_logGeneration != generation)
        {
            g_logGeneration = generation;
            g_logEntries = RuntimeLog::Entries();
            g_logFilterChanged = true;
        }
        if (g_logFilterChanged)
        {
            g_logFilterChanged = false;
            g_logShown.clear();
            for (std::size_t index = 0; index < g_logEntries.size(); ++index)
            {
                if (RuntimeLog::EntryMatches(g_logEntries[index], g_logLowest, g_logSearch))
                    g_logShown.push_back(index);
            }
        }

        const std::string total = std::to_string(g_logEntries.size()) + (g_logEntries.size() == 1 ? " entry" : " entries");
        BeginPage(app, g_logShown.size() == g_logEntries.size() ? total : std::to_string(g_logShown.size()) + " of " + total);
        BeginCard("Runtime Log");

        BeginRow("##log_tools");
        const float toolsStart = ImGui::GetCursorPosX();
        const float toolsWidth = RowWidth();
        if (IconButton(ICON_FA_TRASH "##clear_log", ImGui::GetFrameHeight(), true))
            RuntimeLog::Clear();
        ItemTooltip("Clear Log: empties this list. The log file stays as it is.");
        ImGui::SameLine();

        const char* levelLabel = kLogLevels[0].Label;
        for (const LogLevelChoice& choice : kLogLevels)
        {
            if (choice.Lowest == g_logLowest)
                levelLabel = choice.Label;
        }
        ImGui::SetNextItemWidth(Px(160.0f));
        if (ImGui::BeginCombo("##log_level", levelLabel))
        {
            for (const LogLevelChoice& choice : kLogLevels)
            {
                if (ImGui::Selectable(choice.Label, choice.Lowest == g_logLowest))
                {
                    g_logLowest = choice.Lowest;
                    g_logFilterChanged = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(Px(180.0f));
        FocusOnCtrlF();
        if (ImGui::InputTextWithHint("##log_search", ICON_FA_SEARCH " Search", g_logSearch, sizeof(g_logSearch)))
            g_logFilterChanged = true;
        ItemTooltip("Ctrl+F");
        ImGui::SameLine();
        constexpr const char* kAutoScroll = "Auto-scroll";
        ImGui::SetCursorPosX(toolsStart + toolsWidth - ToggleSwitchWidth() - ImGui::GetStyle().ItemSpacing.x - ImGui::CalcTextSize(kAutoScroll).x);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(kAutoScroll);
        ImGui::SameLine();
        if (ToggleSwitch("##auto_scroll", g_logAutoScroll) && g_logAutoScroll)
            g_logScrolled = ~0ull;
        ItemTooltip("Goes to the newest entry when one comes in.");
        ImGui::PopID();

        BeginRow("##log_view");
        const float height = std::max(ImGui::GetFrameHeight() * 2.0f, ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y - 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.030f, 0.032f, 0.040f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(8.0f, 6.0f));
        ImGui::BeginChild("##console", ImVec2(RowWidth(), height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar);
        SetScrollSurface(ImGui::GetStyleColorVec4(ImGuiCol_ChildBg));
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, Px(6.0f, 2.0f));
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(g_logShown.size()), ImGui::GetTextLineHeightWithSpacing());
        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
            {
                const LogEntry& entry = g_logEntries[g_logShown[static_cast<std::size_t>(index)]];
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 1.0f, 1.0f), "[%s]", entry.Time.c_str());
                ImGui::SameLine();
                ImGui::TextColored(LogColor(entry.Level), "[%s]", RuntimeLog::LevelName(entry.Level));
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.58f, 0.58f, 1.0f), "[%s]", entry.Trace.c_str());
                ImGui::SameLine();
                ImGui::TextUnformatted(entry.Message.c_str());
                ErrorTooltip(entry.Message);
            }
        }
        clipper.End();

        if (g_logAutoScroll && g_logScrolled != generation)
        {
            g_logScrolled = generation;
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopID();

        EndCard();
        EndPageBody();
    }
}
