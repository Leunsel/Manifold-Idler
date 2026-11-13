#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        constexpr double kToastSeconds = 6.0;
        constexpr double kUndoSeconds = 10.0;

        struct Toast
        {
            CreatureId Creature = CreatureId::Slime;
            std::uint32_t Seed = 1u;
            std::string Title;
            std::string Line;
            Page Opens = Page::Creatures;
            std::uint32_t SelectEntry = 0;
            ImVec4 TitleColor = ManifoldTheme::Accent;
        };
        std::deque<Toast> g_toasts;
        double g_toastAt = -1.0;
        std::uint64_t g_undoGeneration = 0;
        double g_undoAt = 0.0;
        float g_undoLift = 0.0f;

        void CollectToasts(IdlerModel& model)
        {
            const std::vector<CreatureId> fresh = model.TakeFreshCreatures();
            if (fresh.size() == 1)
            {
                const Rarity rarity = RarityOf(fresh.back());
                const std::string title = rarity == Rarity::Common ? std::string("New creature!") : "New " + FoldCase(RarityName(rarity)) + " creature!";
                g_toasts.push_back({ fresh.back(), 1u, title, InfoOf(fresh.back()).Name, Page::Creatures, 0, rarity == Rarity::Common ? ManifoldTheme::Accent : RarityColor(rarity) });
            }
            else if (fresh.size() > 1)
                g_toasts.push_back({ fresh.back(), 1u, "New creatures!", std::to_string(fresh.size()) + " joined your collection", Page::Creatures, 0 });

            const std::vector<CreatureId> evolved = model.TakeEvolutionNews();
            if (evolved.size() == 1)
            {
                const CreatureEvolution* evolution = EvolutionOf(evolved[0]);
                g_toasts.push_back({ evolved[0], 1u, "Evolved!", std::string(InfoOf(evolved[0]).Name) + " evolved into " + (evolution ? evolution->Name : ""), Page::Creatures, 0, kBondColor });
            }
            else if (evolved.size() > 1)
                g_toasts.push_back({ evolved.back(), 1u, "Evolved!", std::to_string(evolved.size()) + " creatures evolved", Page::Creatures, 0, kBondColor });
            std::vector<CreatureId> closer = model.TakeBondNews();
            std::erase_if(closer, [&](CreatureId id) { return std::find(evolved.begin(), evolved.end(), id) != evolved.end(); });
            if (closer.size() == 1)
            {
                g_toasts.push_back({ closer[0], 1u, "Closer now", std::string(InfoOf(closer[0]).Name) + ": " + BondName(BondLevel(model.CreatureBondHours(closer[0]))), Page::Creatures, 0, kBondColor });
            }
            else if (closer.size() > 1)
            {
                g_toasts.push_back({ closer.back(), 1u, "Closer now", std::to_string(closer.size()) + " creatures grew closer", Page::Creatures, 0, kBondColor });
            }
            for (const CreatureFamily* family : model.TakeFinishedFamilies())
                g_toasts.push_back({ family->Members.front(), 1u, "Set complete", family->Name, Page::Creatures, 0 });

            std::vector<std::uint32_t> goals;
            bool queueFinished = false;
            bool crashed = false;
            for (const WindowMessage& message : model.TakeWindowMessages())
            {
                if (message.What == WindowMessage::Kind::GoalReached)
                    goals.push_back(message.AppId);
                else if (message.What == WindowMessage::Kind::QueueFinished)
                    queueFinished = true;
                else
                    crashed = true;
            }
            if (model.TakeUpdateNews())
            {
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 4u, "New version", "Version " + model.NewerVersion().value_or(std::string()) + " is out", Page::Settings, 0 });
            }
            if (crashed)
            {
                g_crashLogShown = true;
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 3u, "Last run crashed", "The log shows where", Page::Log, 0, ManifoldTheme::Warning });
            }
            if (goals.size() == 1)
                g_toasts.push_back({ IdleAnimation::CreatureOf(goals[0]), goals[0], "Goal reached", GameName(model, goals[0]), Page::Idling, goals[0] });
            else if (goals.size() > 1)
                g_toasts.push_back({ IdleAnimation::CreatureOf(goals[0]), goals[0], "Goals reached", std::to_string(goals.size()) + " games reached their goal", Page::Idling, 0 });
            if (queueFinished)
            {
                const CreatureId creature = model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());
                g_toasts.push_back({ creature, 2u, "Queue finished", "Every game reached its goal", Page::Idling, 0 });
            }
        }

        void BeginToast(const char* id, float alpha, float lift)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x - Px(18.0f), viewport->Pos.y + viewport->Size.y - Px(18.0f) - lift), ImGuiCond_Always, ImVec2(1.0f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, Px(6.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ManifoldTheme::Card);
            ImGui::PushStyleColor(ImGuiCol_Border, ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.6f));
            ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(4);
        }
    }

    void DrawUndoToast(IdlerModel& model)
    {
        g_undoLift = 0.0f;
        const UndoRecord& record = model.Undoable();
        if (!record.Any())
            return;

        const double now = ImGui::GetTime();
        if (g_undoGeneration != model.UndoGeneration())
        {
            g_undoGeneration = model.UndoGeneration();
            g_undoAt = now;
        }
        const double age = now - g_undoAt;
        if (age > kUndoSeconds)
        {
            model.ForgetUndo();
            return;
        }
        IdleAnimation::RequestFrames();

        const std::size_t games = record.Entries.size();
        const bool wholeList = record.ListPlace >= 0;
        const bool sorted = !record.Order.empty();
        const char* title = sorted ? "Queue sorted" : wholeList ? "List deleted" : games == 1 ? "Game removed" : "Games removed";
        const std::string line = sorted ? std::string(QueueOrderName(record.SortedBy)) : wholeList ? record.List :
            games == 1 ? GameName(model, record.Entries[0].Entry.AppId) : std::to_string(games) + " games from " + record.List;
        const std::uint32_t seed = games > 0 ? record.Entries.back().Entry.AppId : 1u;
        const CreatureId creature = games > 0 ? IdleAnimation::CreatureOf(seed) : model.Buddy() ? *model.Buddy() : IdleAnimation::CreatureOf(model.Account());

        BeginToast("##undo", static_cast<float>(std::min({ 1.0, age / 0.25, (kUndoSeconds - age) / 0.5 })), 0.0f);
        const float pixel = std::round(Px(3.0f));
        const ImVec2 picture = IdleAnimation::SizeAt(pixel);
        const float textWidth = std::min(Px(230.0f), std::max(ImGui::CalcTextSize(title).x, ImGui::CalcTextSize(line.c_str()).x));
        const ImVec2 min = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(picture.x + Px(12.0f) + textWidth, picture.y));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        IdleAnimation::DrawCreature(drawList, min, creature, sorted ? IdleAnimation::Mood::Idling : IdleAnimation::Mood::Sleeping, seed, pixel);
        const float textX = min.x + picture.x + Px(12.0f);
        const float lineHeight = ImGui::GetTextLineHeight();
        const float top = std::round(min.y + (picture.y - lineHeight * 2.0f - Px(3.0f)) * 0.5f);
        DrawCutText(drawList, ImVec2(textX, top), textX + textWidth, ManifoldTheme::Accent, title);
        DrawCutText(drawList, ImVec2(textX, top + lineHeight + Px(3.0f)), textX + textWidth, ManifoldTheme::Text, line);

        ImGui::SameLine(0.0f, Px(14.0f));
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (picture.y - ImGui::GetFrameHeight()) * 0.5f);
        bool undo = ImGui::Button(ICON_FA_UNDO " Undo");
        ItemTooltip(sorted ? "Puts the order back. Also Ctrl+Z." : "Puts it back with its progress. Also Ctrl+Z.");
        if (!ImGui::GetIO().WantTextInput && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Z))
            undo = true;

        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const float barLeft = windowPos.x + Px(12.0f);
        const float barBottom = windowPos.y + windowSize.y - Px(4.0f);
        const float barWidth = (windowSize.x - Px(24.0f)) * static_cast<float>(1.0 - age / kUndoSeconds);
        drawList->AddRectFilled(ImVec2(barLeft, barBottom - Px(2.0f)), ImVec2(barLeft + barWidth, barBottom), ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.7f)), Px(1.0f));

        if (age > 0.25 && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
            g_undoAt = std::max(g_undoAt + ImGui::GetIO().DeltaTime, now - kUndoSeconds + 1.0);
        g_undoLift = windowSize.y + Px(8.0f);
        ImGui::End();

        if (undo)
        {
            model.Undo();
            g_undoLift = 0.0f;
        }
    }

    void DrawToasts(IdlerModel& model)
    {
        CollectToasts(model);
        if (g_toasts.empty())
            return;

        const double now = ImGui::GetTime();
        if (g_toastAt < 0.0)
            g_toastAt = now;
        const double age = now - g_toastAt;
        const double life = g_toasts.size() > 1 ? kToastSeconds * 0.5 : kToastSeconds;
        if (age > life)
        {
            g_toasts.pop_front();
            g_toastAt = -1.0;
            return;
        }
        IdleAnimation::RequestFrames();
        const Toast toast = g_toasts.front();

        BeginToast("##toast", static_cast<float>(std::min({ 1.0, age / 0.25, (life - age) / 0.5 })), g_undoLift);

        const float pixel = std::round(Px(3.0f));
        const ImVec2 picture = IdleAnimation::SizeAt(pixel);
        const float textWidth = std::min(Px(230.0f), std::max(ImGui::CalcTextSize(toast.Title.c_str()).x, ImGui::CalcTextSize(toast.Line.c_str()).x));
        const ImVec2 size(picture.x + Px(12.0f) + textWidth, picture.y);
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("##open", size);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        IdleAnimation::DrawCreature(drawList, min, toast.Creature, IdleAnimation::Mood::Idling, toast.Seed, pixel);
        const float textX = min.x + picture.x + Px(12.0f);
        const float lineHeight = ImGui::GetTextLineHeight();
        const float top = std::round(min.y + (picture.y - lineHeight * 2.0f - Px(3.0f)) * 0.5f);
        DrawCutText(drawList, ImVec2(textX, top), textX + textWidth, toast.TitleColor, toast.Title);
        DrawCutText(drawList, ImVec2(textX, top + lineHeight + Px(3.0f)), textX + textWidth, ManifoldTheme::Text, toast.Line);
        ImGui::End();

        if (clicked)
        {
            g_page = toast.Opens;
            if (toast.Opens == Page::Creatures)
                g_creature = toast.Creature;
            if (toast.SelectEntry != 0)
                g_selectedEntry = toast.SelectEntry;
            g_toasts.pop_front();
            g_toastAt = -1.0;
        }
    }
}
