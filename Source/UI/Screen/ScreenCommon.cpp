#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        struct PageEntry
        {
            Page Id;
            const char* Icon;
            const char* Label;
        };

        constexpr PageEntry kPages[] = {
            { Page::Idling, ICON_FA_PLAY, "Idling" },
            { Page::Library, ICON_FA_GAMEPAD, "Library" },
            { Page::Cards, ICON_FA_CLONE, "Cards" },
            { Page::Statistics, ICON_FA_CHART_BAR, "Statistics" },
            { Page::Creatures, ICON_FA_PAW, "Creatures" },
            { Page::Log, ICON_FA_TERMINAL, "Log" },
            { Page::Settings, ICON_FA_COG, "Settings" },
        };
        constexpr const char* kProgramLabel = kCardFarmEnabled ? "Idler (test)" : "Idler";

        std::string QueueStateLabel(const IdlerModel& model)
        {
            switch (model.Phase())
            {
            case QueuePhase::Starting:
                if (model.WantedCount() > 1)
                    return std::string(ICON_FA_HOURGLASS_HALF "  Starting ") + std::to_string(model.ReadyCount()) + "/" + std::to_string(model.WantedCount());
                return ICON_FA_HOURGLASS_HALF "  Starting";
            case QueuePhase::Stopping:
                return std::string(ICON_FA_HOURGLASS_HALF "  Stopping ") + std::to_string(model.AliveCount());
            case QueuePhase::Running:
                return std::string(ICON_FA_PLAY "  Idling ") + std::to_string(model.RunningCount());
            case QueuePhase::Waiting:
                return ICON_FA_PAUSE "  Waiting";
            default:
                return ICON_FA_PLAY;
            }
        }

        const char* QueueActionLabel(const IdlerModel& model)
        {
            return model.Active() ? ICON_FA_STOP "  Stop" : ICON_FA_PLAY "  Start";
        }

        bool CanSwitchQueue(const IdlerModel& model)
        {
            return model.Phase() != QueuePhase::Stopping && (model.Active() || HasWorkLeft(model.Queue()));
        }

        struct QueueButtonLook
        {
            QueuePhase Phase = QueuePhase::Stopped;
            ImVec4 Color{ 0.0f, 0.0f, 0.0f, 0.0f };
            float Filled = 0.0f;
            float Alive = 0.0f;
            float Working = 0.0f;
            float Pointed = 0.0f;
            int StoppingFrom = 1;
        };
        QueueButtonLook g_queueLook;

        ImVec4 QueueColor(QueuePhase phase, bool can, bool pointed)
        {
            switch (phase)
            {
            case QueuePhase::Stopping:
                return ManifoldTheme::Error;
            case QueuePhase::Starting:
            case QueuePhase::Running:
                return pointed ? ManifoldTheme::Error : ManifoldTheme::Accent;
            case QueuePhase::Waiting:
                return pointed ? ManifoldTheme::Error : ManifoldTheme::Warning;
            default:
                return can ? ManifoldTheme::Accent : ManifoldTheme::Muted;
            }
        }

        ImVec4 Blend(const ImVec4& from, const ImVec4& to, float share)
        {
            return ImVec4(from.x + (to.x - from.x) * share, from.y + (to.y - from.y) * share, from.z + (to.z - from.z) * share, from.w + (to.w - from.w) * share);
        }

        float QueueButtonWidth()
        {
            return std::round(Fonts::Bold()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0.0f, ICON_FA_HOURGLASS_HALF "  Starting 32/32").x + Px(24.0f));
        }

        constexpr int kLogoClicksWanted = 7;
        constexpr double kLogoClickSeconds = 1.5;
        int g_logoClicks = 0;
        double g_logoClickedAt = 0.0;
        float g_pageMarker = -1.0f;

        bool IsWaiting(const IdlerModel& model)
        {
            return model.Active() && model.CurrentHold() != Hold::None;
        }

        bool IsIdling(const IdlerModel& model)
        {
            return !IsWaiting(model) && (model.Active() || model.RunningCount() > 0);
        }

        const char* PageIcon(const IdlerModel& model, const PageEntry& page)
        {
            if (page.Id != Page::Idling)
                return page.Icon;
            return IsIdling(model) ? ICON_FA_PLAY : ICON_FA_PAUSE;
        }

        void CountLogoClick(IdlerModel& model)
        {
            const double now = ImGui::GetTime();
            g_logoClicks = now - g_logoClickedAt < kLogoClickSeconds ? g_logoClicks + 1 : 1;
            g_logoClickedAt = now;
            if (g_logoClicks >= kLogoClicksWanted)
                model.AwardCreature(CreatureId::Manifold);
        }
    }

    bool g_logTabShown = false;
    bool g_crashLogShown = false;
    ImVec2 g_statusCorner;
    std::wstring g_crashTest;

    bool PageShown(Page page)
    {
        if (page == Page::Log)
            return g_logTabShown;
        return kCardFarmEnabled || page != Page::Cards;
    }

    Page g_page = Page::Idling;
    std::uint32_t g_selectedEntry = 0;
    std::uint32_t g_selectedGame = 0;
    std::unordered_set<std::uint32_t> g_gameSelection;

    bool g_webWantsOpen = false;
    bool g_webPreview = false;
    bool g_webDialogOpen = false;

    std::vector<std::uint32_t> g_selection;
    ListEditor g_editor;
    GameWindowState g_gameWindow;

    void OpenGameWindow(std::uint32_t appId, GameTab tab)
    {
        g_gameWindow.WantsOpen = true;
        g_gameWindow.App = appId;
        g_gameWindow.Tab = tab;
        g_gameWindow.StatsAt = -1.0;
        g_gameWindow.EditingStat.clear();
    }

    void Notice(const char* id, const char* icon, ImVec4 color, std::string_view text)
    {
        BeginRow(id);
        const float wrap = ImGui::GetCursorPosX() + RowWidth();
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        if (icon)
        {
            ImGui::TextUnformatted(icon);
            ImGui::SameLine(0.0f, Px(8.0f));
        }
        ImGui::PushTextWrapPos(wrap);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndGroup();
        ErrorTooltip(text);
        ImGui::PopID();
    }

    void EmptyState(const char* id, std::uint32_t seed, std::string_view text)
    {
        BeginRow(id);
        const float width = RowWidth();
        const float pixel = std::round(Px(3.0f));
        const ImVec2 picture = IdleAnimation::SizeAt(pixel);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, picture.y + Px(8.0f)));
        IdleAnimation::DrawCreature(ImGui::GetWindowDrawList(), ImVec2(std::round(start.x + (width - picture.x) * 0.5f), start.y + Px(6.0f)),
            IdleAnimation::CreatureOf(seed), IdleAnimation::Mood::Sleeping, seed, pixel);

        const float wrap = ImGui::GetCursorPosX() + width;
        const float textWidth = ImGui::CalcTextSize(text.data(), text.data() + text.size()).x;
        if (textWidth < width)
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::round((width - textWidth) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
        ImGui::PushTextWrapPos(wrap);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopID();
    }

    bool MainButton(const char* label, float width)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_HeaderHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        const bool clicked = ImGui::Button(label, ImVec2(width, Px(30.0f)));
        ImGui::PopStyleColor(3);
        return clicked;
    }

    void QueueButton(IdlerModel& model, float width, float height, float textScale)
    {
        const QueuePhase phase = model.Phase();
        const bool can = CanSwitchQueue(model);
        const bool working = phase == QueuePhase::Starting || phase == QueuePhase::Stopping;
        const bool alive = phase == QueuePhase::Running || phase == QueuePhase::Waiting;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const ImVec2 max(pos.x + width, pos.y + height);

        const ImVec4 clear(0.0f, 0.0f, 0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, clear);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, clear);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, clear);
        ImGui::BeginDisabled(!can);
        const bool clicked = ImGui::Button("###queue_button", ImVec2(width, height));
        const bool pointed = can && (ImGui::IsItemHovered() || (ImGui::GetIO().NavVisible && ImGui::IsItemFocused()));
        const bool held = can && ImGui::IsItemActive();
        if (can)
            ItemTooltip("Also with Space.");
        else if (phase != QueuePhase::Stopping)
            ItemTooltip("Nothing in the queue can run.");
        ImGui::EndDisabled();
        ImGui::PopStyleColor(3);

        QueueButtonLook& look = g_queueLook;
        if (phase != look.Phase)
        {
            if (phase == QueuePhase::Starting)
                look.Filled = 0.0f;
            else if (phase == QueuePhase::Stopping)
                look.Filled = 1.0f;
            look.StoppingFrom = std::max(1, model.AliveCount());
            look.Phase = phase;
        }
        float filled = 1.0f;
        if (phase == QueuePhase::Starting && model.WantedCount() > 0)
            filled = static_cast<float>(model.ReadyCount()) / static_cast<float>(model.WantedCount());
        else if (phase == QueuePhase::Stopping)
            filled = static_cast<float>(model.AliveCount()) / static_cast<float>(look.StoppingFrom);
        const ImVec4 wanted = QueueColor(phase, can, pointed);
        if (look.Color.w == 0.0f)
            look.Color = wanted;
        const float dt = ImGui::GetIO().DeltaTime;
        const float step = 1.0f - std::exp(-dt * 14.0f);
        const float slow = 1.0f - std::exp(-dt * 5.0f);
        look.Color = Blend(look.Color, wanted, step);
        look.Filled += (filled - look.Filled) * step;
        look.Alive += ((alive ? 1.0f : 0.0f) - look.Alive) * slow;
        look.Working += ((working ? 1.0f : 0.0f) - look.Working) * slow;
        look.Pointed += ((pointed ? 1.0f : 0.0f) - look.Pointed) * step;
        if (working || std::abs((pointed ? 1.0f : 0.0f) - look.Pointed) > 0.004f || std::abs(filled - look.Filled) > 0.002f || std::abs((alive ? 1.0f : 0.0f) - look.Alive) > 0.004f || std::abs((working ? 1.0f : 0.0f) - look.Working) > 0.004f
            || std::abs(wanted.x - look.Color.x) + std::abs(wanted.y - look.Color.y) + std::abs(wanted.z - look.Color.z) > 0.004f)
            IdleAnimation::RequestFrames();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float rounding = Px(5.0f);
        const auto tint = [&](float alpha) { return ImGui::GetColorU32(ManifoldTheme::WithAlpha(look.Color, alpha)); };

        const float calm = 1.0f - look.Alive;
        if (calm > 0.01f)
        {
            drawList->AddRectFilled(pos, max, tint(0.12f * calm), rounding);
            drawList->AddRect(pos, max, tint(0.62f * calm), rounding);
        }
        if (look.Alive > 0.01f)
            DrawIdleHighlight(drawList, pos, max, look.Alive * 1.5f, 0, look.Color);
        if (pointed)
            drawList->AddRectFilled(pos, max, tint(held ? 0.20f : 0.10f), rounding);

        if (look.Working > 0.01f)
        {
            const float split = std::round(pos.x + width * std::clamp(look.Filled, 0.0f, 1.0f));
            if (split > pos.x)
            {
                drawList->PushClipRect(pos, ImVec2(split, max.y), true);
                drawList->AddRectFilled(pos, max, tint(0.26f * look.Working), rounding);
                drawList->PopClipRect();
            }
            if (working && IdleAnimation::Enabled())
            {
                const float band = width * 0.22f;
                const float x = pos.x - band + std::fmod(static_cast<float>(ImGui::GetTime()) * 0.75f, 1.0f) * (width + band * 2.0f);
                DrawSheen(drawList, pos, max, rounding, x, band, ManifoldTheme::WithAlpha(look.Color, 0.30f));
            }
        }

        ImFont* font = Fonts::Bold();
        const float size = std::round(ImGui::GetFontSize() * textScale);
        const ImVec4 ink = Blend(look.Color, ManifoldTheme::Text, phase == QueuePhase::Stopped ? 0.0f : 0.7f);
        const auto drawLabel = [&](const char* label, float alpha)
            {
                if (alpha < 0.01f)
                    return;
                const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
                const ImVec2 textPos(std::round(pos.x + (width - textSize.x) * 0.5f), std::round(pos.y + (height - textSize.y) * 0.5f));
                drawList->AddText(font, size, textPos, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ink, alpha)), label);
            };
        drawLabel(QueueStateLabel(model).c_str(), 1.0f - look.Pointed);
        drawLabel(QueueActionLabel(model), look.Pointed);

        if (clicked)
            model.SetActive(!model.Active());
    }

    void QueueByKey(IdlerModel& model)
    {
        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && !io.NavVisible && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ImGui::IsKeyPressed(ImGuiKey_Space, false) && CanSwitchQueue(model))
            model.SetActive(!model.Active());
    }

    std::string GameName(const IdlerModel& model, std::uint32_t appId)
    {
        const Game* game = model.FindGame(appId);
        return game ? game->Name : "App " + std::to_string(appId);
    }

    std::string ClockText(double seconds)
    {
        const std::time_t when = std::time(nullptr) + static_cast<std::time_t>(seconds);
        std::tm local{};
        if (!LocalTime(when, local))
            return {};
        char text[32];
        std::strftime(text, sizeof(text), "%a %H:%M", &local);
        return text;
    }

    std::string SteamLabel(const IdlerModel& model)
    {
        const Steam::Status& steam = model.SteamStatus();
        if (!steam.Installed())
            return "Steam not found";
        if (!steam.Running())
            return "Steam is closed";
        if (!steam.LoggedOn())
            return "Not logged on";
        return model.Persona().empty() ? "Steam" : model.Persona();
    }
    Page g_shownPage = Page::Idling;
    double g_pageShownAt = -100.0;

    void Sidebar(Shell& app)
    {
        IdlerModel& model = app.Model();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::BeginChild("##sidebar", ImVec2(Px(kSidebarWidth), 0.0f), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, Px(2.0f)));
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const float windowWidth = ImGui::GetWindowWidth();

        ImGui::Dummy(ImVec2(0.0f, Px(14.0f)));
        ImGui::SetCursorPosX(Px(20.0f));
        ImGui::TextColored(ManifoldTheme::Accent, "%s", ICON_FA_CUBE);
        const ImVec2 cubeMin = ImGui::GetItemRectMin();
        const ImVec2 cubeMax = ImGui::GetItemRectMax();
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(cubeMin, cubeMax))
            CountLogoClick(model);
        ImGui::SameLine(0.0f, Px(10.0f));
        ImGui::TextUnformatted("Manifold");
        ImGui::SameLine(0.0f, Px(6.0f));
        ImGui::TextDisabled("%s", kProgramLabel);
        ImGui::Dummy(ImVec2(0.0f, Px(14.0f)));
        const float logoBottom = ImGui::GetCursorScreenPos().y;
        app.AddDragArea(windowPos.x, windowPos.y, windowPos.x + windowWidth, cubeMin.y);
        app.AddDragArea(windowPos.x, cubeMax.y, windowPos.x + windowWidth, logoBottom);
        app.AddDragArea(windowPos.x, cubeMin.y, cubeMin.x, cubeMax.y);
        app.AddDragArea(cubeMax.x, cubeMin.y, windowPos.x + windowWidth, cubeMax.y);

        {
            const ImVec2 first = ImGui::GetCursorScreenPos();
            const float itemHeight = Px(34.0f);
            const float itemStep = itemHeight + ImGui::GetStyle().ItemSpacing.y;
            int row = 0;
            int currentRow = 0;
            for (const PageEntry& entry : kPages)
            {
                if (!PageShown(entry.Id))
                    continue;
                if (entry.Id == g_page)
                    currentRow = row;
                ++row;
            }
            const float target = itemStep * static_cast<float>(currentRow);
            if (g_pageMarker < 0.0f || !IdleAnimation::Enabled())
                g_pageMarker = target;
            g_pageMarker += (target - g_pageMarker) * std::min(1.0f, ImGui::GetIO().DeltaTime * 18.0f);
            if (std::fabs(target - g_pageMarker) < 0.5f)
                g_pageMarker = target;
            else
                IdleAnimation::RequestFrames();

            const ImVec2 markerMin(first.x + Px(8.0f), std::round(first.y + g_pageMarker));
            const ImVec2 markerMax(first.x + windowWidth - Px(8.0f), markerMin.y + itemHeight);
            ImDrawList* markerList = ImGui::GetWindowDrawList();
            markerList->AddRectFilled(markerMin, markerMax, ImGui::GetColorU32(ManifoldTheme::WithAlpha(ManifoldTheme::Accent, 0.08f)), Px(4.0f));
            markerList->AddRectFilled(ImVec2(markerMin.x, markerMin.y + Px(7.0f)), ImVec2(markerMin.x + Px(3.0f), markerMax.y - Px(7.0f)), ImGui::GetColorU32(ManifoldTheme::Accent), Px(1.5f));
        }

        for (const PageEntry& entry : kPages)
        {
            if (!PageShown(entry.Id))
                continue;
            if (SidebarItem(PageIcon(model, entry), entry.Label, g_page == entry.Id))
                g_page = entry.Id;
            const bool waiting = entry.Id == Page::Idling && IsWaiting(model);
            if (waiting || (entry.Id == Page::Idling && IsIdling(model)) || (entry.Id == Page::Creatures && model.HasNewCreatures()))
            {
                const ImVec2 itemMin = ImGui::GetItemRectMin();
                const ImVec2 itemMax = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(itemMax.x - Px(18.0f), (itemMin.y + itemMax.y) * 0.5f), Px(3.0f),
                    ImGui::GetColorU32(waiting ? ManifoldTheme::Warning : ManifoldTheme::Accent));
            }
        }

        const float footerY = ImGui::GetWindowHeight() - Px(52.0f);

        const float terrariumTop = ImGui::GetCursorPosY() + Px(10.0f);
        const float terrariumHeight = footerY - Px(10.0f) - terrariumTop;
        const bool homeShown = g_page == Page::Creatures && model.Options().CreatureHome;
        if (model.Options().ShowTerrarium && !homeShown && terrariumHeight >= Px(90.0f))
        {
            ImGui::SetCursorPos(ImVec2(Px(12.0f), terrariumTop));
            DrawTerrarium(model, ImVec2(std::round(windowWidth - Px(24.0f)), std::round(terrariumHeight)));
        }

        if (ImGui::GetCursorPosY() < footerY)
            ImGui::SetCursorPosY(footerY);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const float width = ImGui::GetContentRegionAvail().x;
        ImGui::Dummy(ImVec2(0.0f, Px(6.0f)));

        const bool ready = model.SteamStatus().LoggedOn();
        const int running = model.RunningCount();
        const float footerHeight = Px(30.0f);
        const float reloadWidth = Px(30.0f);
        const float labelWidth = width - reloadWidth - Px(10.0f);
        const ImVec2 labelPos = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton("##steam", ImVec2(labelWidth, footerHeight)))
            Desktop::OpenSteam();
        const ImVec2 badgeMin(labelPos.x + Px(8.0f), labelPos.y);
        const ImVec2 badgeMax(labelPos.x + labelWidth, labelPos.y + footerHeight);
        if (ImGui::IsItemHovered())
            drawList->AddRectFilled(badgeMin, badgeMax, IM_COL32(255, 255, 255, 9), Px(4.0f));
        const std::string badgeTip = running > 0 ? std::to_string(running) + (running == 1 ? " game idling. Opens Steam." : " games idling. Opens Steam.") : std::string("Opens Steam.");
        ItemTooltip(badgeTip.c_str());

        const float glow = Fade(ImGui::GetID("##badge_glow"), running > 0, 3.0f);
        if (glow > 0.0f)
        {
            DrawIdleHighlight(drawList, badgeMin, badgeMax, glow);
            IdleAnimation::RequestFrames();
        }
        const float labelY = labelPos.y + (footerHeight - ImGui::GetTextLineHeight()) * 0.5f;
        if (running > 0)
        {
            const ImVec2 at(labelPos.x + Px(12.0f), std::round(labelPos.y + (footerHeight - IdleAnimation::Height()) * 0.5f));
            if (const auto buddy = model.Buddy())
                IdleAnimation::DrawCreature(drawList, at, *buddy, IdleAnimation::Mood::Idling, model.Account(), IdleAnimation::PixelSize());
            else
                IdleAnimation::Draw(drawList, at, IdleAnimation::Mood::Idling, model.Account());
        }
        else if (const ImTextureID avatar = ready ? Covers::Avatar(model.SteamId()) : ImTextureID{})
        {
            const float side = std::round(Px(22.0f));
            const ImVec2 at(labelPos.x + Px(17.0f), std::round(labelPos.y + (footerHeight - side) * 0.5f));
            drawList->AddImageRounded(avatar, at, ImVec2(at.x + side, at.y + side), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f - glow)), Px(4.0f));
        }
        else
        {
            drawList->AddText(ImVec2(labelPos.x + Px(23.0f), labelY), ready ? IM_COL32(112, 116, 124, 255) : ImGui::GetColorU32(ManifoldTheme::Warning),
                ready ? ICON_FA_USER : ICON_FA_EXCLAMATION_TRIANGLE);
        }
        const std::string label = SteamLabel(model);
        drawList->PushClipRect(labelPos, ImVec2(labelPos.x + labelWidth - Px(4.0f), labelPos.y + footerHeight), true);
        DrawCutText(drawList, ImVec2(labelPos.x + Px(50.0f), labelY), labelPos.x + labelWidth - Px(4.0f), !ready ? ManifoldTheme::Warning : running > 0 ? ManifoldTheme::Text : ManifoldTheme::Label, label);
        drawList->PopClipRect();

        ImGui::SameLine(0.0f, 0.0f);
        if (IconLink("##reload", ICON_FA_SYNC, ImVec2(reloadWidth, footerHeight), "Reads the library again."))
            model.Rescan();

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    bool BeginPage(Shell& app, std::string_view status, ImVec4 statusColor, bool queueButton, const char* statusTip)
    {
        const PageEntry* page = &kPages[0];
        for (const PageEntry& entry : kPages)
        {
            if (entry.Id == g_page)
                page = &entry;
        }

        const float button = ImGui::GetFrameHeight();
        const float gap = Px(2.0f);
        const float buttons = button * 3.0f + gap * 2.0f;
        const float headerHeight = Px(34.0f);
        const float queueHeight = Px(26.0f);
        const float queueWidth = queueButton ? QueueButtonWidth() : 0.0f;
        const float queueGap = queueButton ? Px(10.0f) : 0.0f;
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const HeaderStatus statusArea = PageHeader(PageIcon(app.Model(), *page), page->Label, status, statusColor, buttons + queueWidth + queueGap);
        const ImVec2 body = ImGui::GetCursorScreenPos();

        const float buttonsX = pos.x + width - Px(6.0f) - buttons;
        const float queueX = buttonsX - queueGap - queueWidth;
        if (queueButton)
        {
            ImGui::SetCursorScreenPos(ImVec2(queueX, pos.y + std::round((headerHeight - queueHeight) * 0.5f)));
            QueueButton(app.Model(), queueWidth, queueHeight);
        }

        ImGui::SetCursorScreenPos(ImVec2(buttonsX, pos.y + std::round((headerHeight - button) * 0.5f)));
        if (IconLink("##minimise", ICON_FA_MINUS, ImVec2(button, button), nullptr))
            app.Minimise();
        ImGui::SameLine(0.0f, gap);
        if (IconLink("##maximise", app.Maximised() ? ICON_FA_WINDOW_RESTORE : ICON_FA_WINDOW_MAXIMIZE, ImVec2(button, button), nullptr))
            app.ToggleMaximised();
        ImGui::SameLine(0.0f, gap);
        if (IconLink("##close", ICON_FA_TIMES, ImVec2(button, button), nullptr, ManifoldTheme::Error))
            app.Close();

        const ImVec2 area = ImGui::GetWindowPos();
        const float headerBottom = pos.y + headerHeight;
        bool clicked = false;
        if (statusTip && statusArea.Shown)
        {
            const ImVec2 min(std::floor(statusArea.Min.x - Px(4.0f)), pos.y + Px(4.0f));
            const ImVec2 max(std::ceil(statusArea.Max.x + Px(4.0f)), headerBottom - Px(4.0f));
            ImGui::SetCursorScreenPos(min);
            clicked = ImGui::InvisibleButton("##page_status", ImVec2(max.x - min.x, max.y - min.y));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                const float line = std::round(statusArea.Max.y + Px(1.0f));
                ImGui::GetWindowDrawList()->AddLine(ImVec2(statusArea.Min.x, line), ImVec2(statusArea.Max.x, line), ImGui::GetColorU32(statusColor), Px(1.0f));
            }
            ItemTooltip(statusTip);
            g_statusCorner = ImVec2(max.x, headerBottom);
            app.AddDragArea(area.x, area.y, min.x, headerBottom);
            app.AddDragArea(max.x, area.y, queueX, headerBottom);
            app.AddDragArea(min.x, area.y, max.x, min.y);
            app.AddDragArea(min.x, max.y, max.x, headerBottom);
        }
        else
        {
            app.AddDragArea(area.x, area.y, queueX, headerBottom);
        }
        app.AddDragArea(queueX, area.y, area.x + ImGui::GetWindowWidth(), pos.y);
        ImGui::SetCursorScreenPos(body);
        BeginPageBody();
        return clicked;
    }
}
