#include "UI/Screen/Screen.h"

namespace Idler::UI::Screen
{
    namespace
    {
        constexpr float kSettingsWidth = 1120.0f;

        void SteamCard(IdlerModel& model)
        {
            const Steam::Status& steam = model.SteamStatus();
            BeginCard("Steam");
            InfoRow("State", steam.LoggedOn() ? "Running" : SteamLabel(model), steam.LoggedOn() ? ManifoldTheme::Accent : ManifoldTheme::Warning);
            if (!model.Persona().empty())
                InfoRow("Account", model.Persona(), ManifoldTheme::Label);
            if (steam.Installed())
                InfoRow("Folder", WideToUtf8(steam.Root.wstring()), ManifoldTheme::Muted);
            if (!model.ScanProblem().empty())
                Notice("##problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Warning, model.ScanProblem());

            BeginRow("##actions");
            if (!steam.Running())
            {
                ImGui::BeginDisabled(!steam.Installed());
                if (ActionButton(ICON_FA_POWER_OFF, "Start Steam"))
                    Desktop::OpenSteam();
                ImGui::EndDisabled();
            }
            ImGui::BeginDisabled(!steam.LoggedOn() || model.Scanning());
            if (ActionButton(ICON_FA_SYNC, "Read library again"))
                model.Rescan();
            ImGui::EndDisabled();
            EndActionButtons();
            ImGui::PopID();
            EndCard();
        }

        void HourRow(const char* label, int& hour, bool& changed)
        {
            ControlRow(label, nullptr, Px(110.0f), [&]()
                {
                    const int step = 1;
                    ImGui::PushID(label);
                    if (ImGui::InputScalar("##hour", ImGuiDataType_S32, &hour, &step, nullptr, "%d:00"))
                    {
                        hour = std::clamp(hour, 0, 23);
                        changed = true;
                    }
                    ImGui::PopID();
                });
        }

        void WhenCard(IdlerModel& model)
        {
            Settings& options = model.Options();
            bool changed = false;

            BeginCard("When to idle");
            if (Desktop::KnowsInputIdle())
                changed |= ToggleRow("Only when I am away", "Idling starts after this long without mouse or keyboard, and stops at the next input.", options.OnlyWhenAway);
            if (options.OnlyWhenAway && Desktop::KnowsInputIdle())
            {
                ControlRow("Away for", nullptr, Px(110.0f), [&]()
                    {
                        const float step = 1.0f;
                        const float fastStep = 5.0f;
                        if (ImGui::InputScalar("##away", ImGuiDataType_Float, &options.AwayMinutes, &step, &fastStep, "%.0f min"))
                        {
                            options.AwayMinutes = std::clamp(options.AwayMinutes, 1.0f, 1440.0f);
                            changed = true;
                        }
                    });
            }
            changed |= ToggleRow("Only at certain hours", "Idling only runs from the first hour up to the second, also over midnight. Your computer's clock counts.", options.OnlyInHours);
            if (options.OnlyInHours)
            {
                HourRow("From", options.FromHour, changed);
                HourRow("Until", options.ToHour, changed);
            }
            EndCard();

            if (changed)
                model.SaveNow();
        }

        void BehaviourCard(Shell& app)
        {
            IdlerModel& model = app.Model();
            Settings& options = model.Options();
            bool changed = false;

            BeginCard("Idling");
            changed |= ToggleRow("Pause while a game runs", "Stops idling while Steam runs another game, and goes on afterwards.", options.PauseWhilePlaying);
            changed |= ToggleRow("Continue after a restart", "When the program was idling as it closed, it starts idling again on its next start.", options.ResumeOnStart);
            changed |= ToggleRow("Keep the computer awake", "No standby while games idle. The screen may still go off.", options.KeepAwake);
            changed |= ToggleRow("Terrarium", "The creatures of the games that idle live in the sidebar. A click pets one.", options.ShowTerrarium);
            ControlRow("Season", "The time of year in the terrarium and in the creatures' home.", Px(140.0f), [&]()
                {
                    changed |= ChoiceCombo("##season", options.Season, SceneSeason::Count, SceneSeasonName, Px(140.0f));
                });
            if (Desktop::CanMovePointer())
                changed |= ToggleRow("Creatures may tug the mouse", "One that is angry after a throw and reaches the pointer pulls it a little way off. Moving the mouse frees it.",
                    options.CreaturesTugMouse);
            changed |= ToggleRow("Log tab", "Shows what the program does while it runs. The file is written either way.", options.ShowLog);
            changed |= ToggleRow("Vary the times",
                "Games start at different moments and take breaks of their own, and their goals differ a little, so their hours do not come out the same.", options.VaryTimes);
            if (options.VaryTimes)
            {
                ControlRow("Goals differ by", "How far a game's goal may lie from the one you set, up or down.", Px(110.0f), [&]()
                    {
                        const float step = 5.0f;
                        if (ImGui::InputScalar("##spread", ImGuiDataType_Float, &options.VarySpread, &step, nullptr, "%.0f %%"))
                        {
                            options.VarySpread = std::clamp(options.VarySpread, 5.0f, 50.0f);
                            changed = true;
                        }
                    });
            }
            changed |= ToggleRow("Protect refunds",
                "Steam refunds a game within 14 days of buying it, with under 2 hours played. Such a game stops 10 minutes before that limit.", options.RefundGuard);
            changed |= ToggleRow("Invisible while idling",
                "You appear offline to friends while games idle. Afterwards your status goes back to online, also if you had set another one.", options.InvisibleWhileIdling);
            EndCard();

            BeginCard("Window");
            if (app.HasTray())
                changed |= ToggleRow("Keep idling in the tray", "Closing the window while games idle moves it to the tray instead of ending the program.", options.KeepInTray);
            bool startsAtLogin = Desktop::StartsAtLogin();
            if (ToggleRow(Desktop::StartAtLoginLabel(), Desktop::StartAtLoginNote(), startsAtLogin))
                Desktop::SetStartsAtLogin(startsAtLogin);
            EndCard();

            if (changed)
                model.SaveNow();
        }

        constexpr double kBackupListSeconds = 5.0;
        constexpr double kBackupNoticeSeconds = 8.0;

        struct BackupDialogState
        {
            bool WantsOpen = false;
            std::vector<Backups::Backup> List;
            std::vector<std::optional<Backups::Contents>> Contents;
            int Chosen = -1;
            std::string Problem;
        };

        BackupDialogState g_backups;
        std::vector<Backups::Backup> g_backupList;
        double g_backupListAt = -100.0;
        std::string g_backupNotice;
        bool g_backupNoticeProblem = false;
        double g_backupNoticeAt = -100.0;

        std::string FileNameText(const std::filesystem::path& path)
        {
            const std::u8string text = path.filename().u8string();
            return std::string(text.begin(), text.end());
        }

        std::string BackupDayText(std::time_t time)
        {
            const std::time_t now = std::time(nullptr);
            std::tm local{};
            std::tm today{};
            std::tm yesterday{};
            if (!LocalTime(time, local) || !LocalTime(now, today) || !LocalTime(now - 86400, yesterday))
                return {};
            const auto sameDay = [](const std::tm& first, const std::tm& second) { return first.tm_year == second.tm_year && first.tm_yday == second.tm_yday; };
            if (sameDay(local, today))
                return "Today";
            if (sameDay(local, yesterday))
                return "Yesterday";
            char text[40];
            const std::size_t length = std::strftime(text, sizeof(text), "%a %d %b %Y", &local);
            return std::string(text, length);
        }

        std::string BackupWhenText(const Backups::Backup& backup)
        {
            std::string text = BackupDayText(backup.Time);
            std::tm local{};
            if (backup.Kind != Backups::BackupKind::Daily && LocalTime(backup.Time, local))
            {
                char clock[16];
                const std::size_t length = std::strftime(clock, sizeof(clock), "%H:%M", &local);
                text += ", " + std::string(clock, length);
            }
            return text;
        }

        const char* BackupKindText(Backups::BackupKind kind)
        {
            switch (kind)
            {
            case Backups::BackupKind::Daily:
                return "Daily";
            case Backups::BackupKind::Saved:
                return "By hand";
            case Backups::BackupKind::BeforeRestore:
                return "Before a restore";
            }
            return "";
        }

        const char* BackupIcon(Backups::BackupKind kind)
        {
            switch (kind)
            {
            case Backups::BackupKind::Daily:
                return ICON_FA_CALENDAR_DAY;
            case Backups::BackupKind::Saved:
                return ICON_FA_SAVE;
            case Backups::BackupKind::BeforeRestore:
                return ICON_FA_UNDO;
            }
            return ICON_FA_ARCHIVE;
        }

        void NoteBackup(const Backups::Result& result)
        {
            g_backupNoticeAt = ImGui::GetTime();
            g_backupListAt = -100.0;
            g_backupNoticeProblem = !result.Ok();
            switch (result.What)
            {
            case Backups::Outcome::Made:
                g_backupNotice = "Backed up as " + FileNameText(result.File) + ".";
                break;
            case Backups::Outcome::Unchanged:
                g_backupNotice = "Nothing changed since the last backup.";
                break;
            case Backups::Outcome::NothingToBackUp:
                g_backupNotice = "Nothing is saved yet that could be backed up.";
                break;
            case Backups::Outcome::Failed:
                g_backupNotice = result.Problem;
                break;
            }
        }

        void OpenBackupDialog(const IdlerModel& model)
        {
            g_backups = {};
            g_backups.WantsOpen = true;
            g_backups.List = Backups::List(model.SettingsPath());
            for (const Backups::Backup& backup : g_backups.List)
                g_backups.Contents.push_back(Backups::Inspect(model.SettingsPath(), backup.File));
            g_backups.Chosen = g_backups.List.empty() ? -1 : 0;
        }

        void BackupDialog(Shell& app)
        {
            IdlerModel& model = app.Model();
            if (g_backups.WantsOpen)
            {
                ImGui::OpenPopup("##backups");
                g_backups.WantsOpen = false;
            }
            const std::size_t count = g_backups.List.size();
            const std::string note = std::to_string(count) + (count == 1 ? " backup" : " backups");
            if (!BeginDialog("##backups", ICON_FA_HISTORY, "Restore a backup", Px(660.0f, 480.0f), note))
                return;

            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
            ImGui::TextUnformatted("The program starts again with the backup you choose. What it holds now is backed up first, so you can go back to it.");
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            const float spacing = ImGui::GetStyle().ItemSpacing.y;
            const float footer = ImGui::GetFrameHeight() + spacing + (g_backups.Problem.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing() * 2.0f + spacing);
            BeginCardList("##backup_list", footer);
            for (std::size_t index = 0; index < count; ++index)
            {
                const Backups::Backup& backup = g_backups.List[index];
                const std::optional<Backups::Contents>& contents = g_backups.Contents[index];
                std::vector<RowTag> tags;
                if (!contents)
                {
                    tags.push_back({ "Cannot be read", ManifoldTheme::Warning });
                }
                else
                {
                    if (contents->HasSettings)
                        tags.push_back({ std::to_string(contents->Creatures) + (contents->Creatures == 1 ? " creature" : " creatures"), ManifoldTheme::Label });
                    if (contents->HasStats)
                        tags.push_back({ ShortDurationText(static_cast<std::uint64_t>(contents->Seconds)) + " idled", ManifoldTheme::Label });
                }
                tags.push_back({ BackupKindText(backup.Kind), ManifoldTheme::Muted });
                const bool chosen = g_backups.Chosen == static_cast<int>(index);
                ImGui::PushID(static_cast<int>(index));
                if (ListRow("##backup", chosen, BackupIcon(backup.Kind), chosen ? ManifoldTheme::Accent : ManifoldTheme::Muted, BackupWhenText(backup),
                        contents ? ManifoldTheme::Text : ManifoldTheme::Muted, tags))
                {
                    g_backups.Chosen = static_cast<int>(index);
                    g_backups.Problem.clear();
                }
                ImGui::PopID();
            }
            EndCardList();

            if (!g_backups.Problem.empty())
                Notice("##restore_problem", ICON_FA_EXCLAMATION_TRIANGLE, ManifoldTheme::Error, g_backups.Problem);

            const bool readable = g_backups.Chosen >= 0 && static_cast<std::size_t>(g_backups.Chosen) < count &&
                g_backups.Contents[static_cast<std::size_t>(g_backups.Chosen)].has_value();
            ImGui::BeginDisabled(!readable || model.SavesBlocked());
            if (ImGui::Button(ICON_FA_HISTORY "  Restore and start again"))
            {
                const Backups::Result result = model.RestoreBackup(g_backups.List[static_cast<std::size_t>(g_backups.Chosen)].File);
                if (result.Ok())
                {
                    ImGui::CloseCurrentPopup();
                    app.Restart();
                }
                else
                {
                    g_backups.Problem = result.Problem;
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        void BackupsCard(Shell& app)
        {
            IdlerModel& model = app.Model();
            const double now = ImGui::GetTime();
            if (now - g_backupListAt > kBackupListSeconds)
            {
                g_backupList = Backups::List(model.SettingsPath());
                g_backupListAt = now;
            }

            BeginCard("Backups");
            if (g_backupList.empty())
            {
                InfoRow("Last backup", "None yet", ManifoldTheme::Muted);
            }
            else
            {
                InfoRow("Last backup", BackupWhenText(g_backupList.front()));
                std::string kept = std::to_string(g_backupList.size());
                if (g_backupList.size() > 1)
                {
                    std::string oldest = BackupDayText(g_backupList.back().Time);
                    if (oldest == "Today" || oldest == "Yesterday")
                        oldest[0] = static_cast<char>(oldest[0] - 'A' + 'a');
                    kept += ", since " + oldest;
                }
                InfoRow("Kept", kept, ManifoldTheme::Label);
            }
            ItemTooltip("Each day the program runs, it backs up its settings and statistics as a ZIP next to them. The last 7 days are kept, "
                        "and the last 5 made by hand or before a restore.");
            if (!g_backupNotice.empty() && now - g_backupNoticeAt < kBackupNoticeSeconds)
                Notice("##backup_notice", g_backupNoticeProblem ? ICON_FA_EXCLAMATION_TRIANGLE : ICON_FA_CHECK, g_backupNoticeProblem ? ManifoldTheme::Warning : ManifoldTheme::Accent,
                    g_backupNotice);

            BeginRow("##backup_actions");
            ImGui::BeginDisabled(model.SavesBlocked());
            if (ActionButton(ICON_FA_SAVE, "Back up now"))
                NoteBackup(model.BackUpNow());
            ImGui::BeginDisabled(g_backupList.empty());
            if (ActionButton(ICON_FA_HISTORY, "Restore a backup"))
                OpenBackupDialog(model);
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            if (ActionButton(ICON_FA_FOLDER_OPEN, "Open the folder"))
            {
                const std::filesystem::path folder = Backups::FolderOf(model.SettingsPath());
                std::error_code ignored;
                std::filesystem::create_directories(folder, ignored);
                Desktop::OpenFolder(folder);
            }
            EndActionButtons();
            ImGui::PopID();
            EndCard();

            BackupDialog(app);
        }

        void UpdatesPopup(IdlerModel& model)
        {
            ImGui::SetNextWindowSize(ImVec2(Px(400.0f), 0.0f));
            ImGui::SetNextWindowPos(g_statusCorner, ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, Px(12.0f, 10.0f));
            const bool open = ImGui::BeginPopup("##updates");
            ImGui::PopStyleVar();
            if (!open)
                return;

            Settings& options = model.Options();
            if (ToggleRow("Look for new versions", nullptr, options.UpdateCheck))
                model.SaveNow();
            ImGui::PushStyleColor(ImGuiCol_Text, ManifoldTheme::Muted);
            ImGui::PushTextWrapPos(ImGui::GetContentRegionMax().x);
            ImGui::TextUnformatted("Once a day the program asks GitHub for its newest release. The question carries only the program's version, and nothing is downloaded.");
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            if (const std::optional<std::string> newer = model.NewerVersion())
            {
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
                Notice("##newer", ICON_FA_GIFT, ManifoldTheme::Accent, "Version " + *newer + " is out.");
                const std::string page = Releases::PageOf(*newer);
                if (ImGui::Button(ICON_FA_EXTERNAL_LINK_ALT "  Open the release page") && Releases::IsReleasePage(page))
                {
                    Desktop::OpenWebPage(page);
                    ImGui::CloseCurrentPopup();
                }
            }
            else if (options.UpdateCheck && options.UpdateCheckedAt != 0)
            {
                ImGui::Dummy(ImVec2(0.0f, Px(2.0f)));
                ImGui::TextDisabled("Last looked on %s: none is newer.", DateText(options.UpdateCheckedAt).c_str());
            }
            ImGui::EndPopup();
        }

        void NewGamesCard(IdlerModel& model)
        {
            bool changed = false;
            BeginCard("New games in the queue");
            GoalControls("##default_goal", model.Options().DefaultGoal, changed);
            EndCard();
            if (changed)
                model.SaveNow();
        }
    }

    void SettingsPage(Shell& app)
    {
        const std::optional<std::string> newer = app.Model().NewerVersion();
        const std::string version = std::string("Version ") + kVersion + (newer ? ", " + *newer + " is out" : std::string());
        const bool versionClicked = BeginPage(app, version, newer ? ManifoldTheme::Accent : ManifoldTheme::Muted, true, "Look for new versions");
        if (versionClicked)
            ImGui::OpenPopup("##updates");
        UpdatesPopup(app.Model());
        ImGui::BeginChild("##settings_width", ImVec2(std::min(ImGui::GetContentRegionAvail().x, Px(kSettingsWidth)), 0.0f), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoBackground);
        SetScrollSurface(ManifoldTheme::Page);
        CardColumns("##settings_columns",
            [&]()
            {
                SteamCard(app.Model());
                WhenCard(app.Model());
                NewGamesCard(app.Model());
                BackupsCard(app);
            },
            [&]() { BehaviourCard(app); });
        ImGui::EndChild();
        EndPageBody();
    }
}
