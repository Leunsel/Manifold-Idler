#include "BackupCommands.h"
#include "Core/Errors.h"
#include "Core/Text.h"
#include "Ipc.h"
#include "Paths.h"
#include "Request.h"
#include "Serve.h"
#include "Service.h"
#include "Steam/Worker.h"
#include "Themes.h"

#include "CLI11.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <clocale>
#include <csignal>
#endif

#include <cstdio>
#include <deque>
#include <string>
#include <vector>

namespace
{
    constexpr int kNoServer = 2;

    bool IsWorkerArgument(const std::string& argument)
    {
        return argument == "--scan" || argument.rfind("--idle=", 0) == 0 || argument.rfind("--achievements=", 0) == 0;
    }

    int Ask(const std::filesystem::path& settings, const std::vector<std::string>& words)
    {
        std::string reply;
        if (!Idler::Cli::Ask(Idler::Cli::SocketFile(settings), Idler::Cli::JoinRequest(words), reply))
        {
            std::fprintf(stderr, "%s\n", Idler::ErrorMessage(Idler::ErrorCode::NoServer).c_str());
            return kNoServer;
        }
        bool ok = false;
        std::string text;
        if (!Idler::Cli::ReadReply(reply, ok, text))
        {
            std::fprintf(stderr, "%s\n", Idler::ErrorMessage(Idler::ErrorCode::BadReply).c_str());
            return 1;
        }
        if (!text.empty() && text.back() != '\n')
            text.push_back('\n');
        std::fputs(text.c_str(), ok ? stdout : stderr);
        return ok ? 0 : 1;
    }

    int Explain(const std::string& code, bool markdown)
    {
        if (markdown)
        {
            std::fputs(Idler::ErrorsMarkdown().c_str(), stdout);
            return 0;
        }
        if (code.empty())
        {
            std::fputs(Idler::ErrorList().c_str(), stdout);
            return 0;
        }
        const Idler::ErrorInfo* info = Idler::FindError(code);
        if (!info)
        {
            std::fprintf(stderr, "No error has the code %s. All codes: idler explain\n", code.c_str());
            return 1;
        }
        std::fputs(Idler::ErrorReference(*info).c_str(), stdout);
        return 0;
    }
}

int main(int argc, char** argv)
{
    if (argc == 2 && IsWorkerArgument(argv[1]))
    {
        const std::wstring command = Idler::Utf8ToWide(argv[1]);
        return Idler::Worker::Run(command.c_str());
    }

#ifdef _WIN32
    ::SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32 | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
    ::SetConsoleOutputCP(CP_UTF8);
#else
    std::signal(SIGPIPE, SIG_IGN);
    std::setlocale(LC_CTYPE, "C.UTF-8");
#endif

    CLI::App app{ "Manifold Idler without a window. A server keeps the queue going, the other commands talk to it.\n"
                  "Steam has to run on this computer, logged on, as this user.", "idler" };
    argv = app.ensure_utf8(argv);
    app.require_subcommand(1);
    app.set_version_flag("--version", "idler 0.1.0");

    std::string settingsText;
    app.add_option("--settings", settingsText, "The settings file, also of the window program");

    Idler::Cli::ServeOptions serveOptions;
    CLI::App* serve = app.add_subcommand("serve", "Runs the idler in this terminal until it is told to end");
    serve->add_flag("--start", serveOptions.Start, "Start the queue as soon as the library is read");
    serve->add_flag("--pretend", serveOptions.Pretend, "")->group("");
    CLI::Option* serveWeb = serve->add_option("--web", serveOptions.WebAddress, "Also serve a web page, at 127.0.0.1:8090 or the address:port given")->expected(0, 1);
    std::string themesText;
    serve->add_option("--themes", themesText, "The folder with themes of your own for the web page; Manifold-Idler.themes beside the settings without it");

    std::vector<std::string> request;
    const auto simple = [&](const char* name, const char* help)
        {
            CLI::App* command = app.add_subcommand(name, help);
            command->callback([&request, name]() { request = { name }; });
            return command;
        };
    simple("status", "What the idler is doing");
    simple("queue", "The games in the queue");
    simple("start", "Start the queue");
    simple("stop", "Stop the queue");
    simple("lists", "The saved lists; the one with a star is the queue");
    simple("web", "The address of the web page, with its token");
    simple("quit", "End the server");

    std::vector<std::uint32_t> apps;
    float forHours = 0.0f;
    float untilHours = 0.0f;
    CLI::App* add = app.add_subcommand("add", "Add games to the queue");
    add->add_option("appid", apps, "App ids")->required();
    CLI::Option* forOption = add->add_option("--for", forHours, "Idle them for this many hours");
    add->add_option("--until", untilHours, "Idle them until this many hours are played")->excludes(forOption);
    add->callback([&]()
        {
            request = { "add" };
            if (add->count("--for") > 0)
                request.insert(request.end(), { "for", std::to_string(forHours) });
            else if (add->count("--until") > 0)
                request.insert(request.end(), { "until", std::to_string(untilHours) });
            request.emplace_back("--");
            for (const std::uint32_t appId : apps)
                request.push_back(std::to_string(appId));
        });

    std::vector<std::uint32_t> removeApps;
    CLI::App* remove = app.add_subcommand("remove", "Take games out of the queue");
    remove->add_option("appid", removeApps, "App ids")->required();
    remove->callback([&]()
        {
            request = { "remove" };
            for (const std::uint32_t appId : removeApps)
                request.push_back(std::to_string(appId));
        });

    std::uint32_t goalApp = 0;
    std::vector<std::string> goalWords;
    CLI::App* goal = app.add_subcommand("goal", "Set a game's goal: none, for <hours> or until <hours>");
    goal->add_option("appid", goalApp, "App id")->required();
    goal->add_option("goal", goalWords, "none, for <hours> or until <hours>")->required();
    goal->callback([&]()
        {
            request = { "goal", std::to_string(goalApp) };
            request.insert(request.end(), goalWords.begin(), goalWords.end());
        });

    int slots = 0;
    CLI::App* slotsCommand = app.add_subcommand("slots", "How many games idle at once");
    slotsCommand->add_option("count", slots, "From 1 to 32")->required();
    slotsCommand->callback([&]() { request = { "slots", std::to_string(slots) }; });

    std::string search;
    int gameLimit = 50;
    CLI::App* games = app.add_subcommand("games", "The games of this account");
    games->add_option("search", search, "A part of the name, or an app id");
    games->add_option("--limit", gameLimit, "Show at most this many")->check(CLI::PositiveNumber);
    games->callback([&]() { request = { "games", std::to_string(gameLimit), search }; });

    std::string listName;
    CLI::App* use = app.add_subcommand("use", "Make a saved list the queue");
    use->add_option("list", listName, "Its name")->required();
    use->callback([&]() { request = { "use", listName }; });

    int logLines = 30;
    CLI::App* log = app.add_subcommand("log", "What the idler did last");
    log->add_option("lines", logLines, "How many lines")->check(CLI::PositiveNumber);
    log->callback([&]() { request = { "log", std::to_string(logLines) }; });

    std::deque<std::vector<std::string>> passedWords;
    const auto passing = [&](const char* name, const char* help, const char* wordsHelp, bool required)
        {
            std::vector<std::string>& words = passedWords.emplace_back();
            CLI::App* command = app.add_subcommand(name, help);
            CLI::Option* option = command->add_option("words", words, wordsHelp);
            if (required)
                option->required();
            command->callback([&request, &words, name]()
                {
                    request = { name };
                    request.insert(request.end(), words.begin(), words.end());
                });
            return command;
        };
    passing("move", "Move a game in the queue: move <appid> <place>", "The app id and the place, from 1", true);
    passing("sort", "Sort the queue by name, left, least or most", "name, left, least or most", true);
    passing("now", "Put a game first and start the queue", "The app id", true);
    passing("retry", "Try a game again that did not start", "The app id", true);
    passing("refund", "Let a game idle past the refund limit", "The app id", true);
    passing("clear", "Take games out of the queue: finished or all", "finished or all", true);
    simple("undo", "Undo the last removal, sort or deleted list");
    passing("list", "Change the saved lists: list new <name>, list rename <name> <new name>, list delete <name>", "new, rename or delete, then the names", true);
    simple("rescan", "Read the library again");
    passing("game", "Everything about one game", "The app id", true);
    passing("stats", "What was idled; stats games [time, sessions, longest, days or recent] lists the games", "Nothing, or games and an order", false);
    passing("history", "Idle time by day; with a day such as 2026-11-12, that day's games", "A day", false);
    simple("settings", "The settings the server goes by; set changes them");
    passing("set", "Change a setting: set <setting> <value>", "The setting and its value", true);

    std::string libraryShow = "all";
    std::string librarySort = "name";
    std::string librarySearch;
    int libraryRating = 0;
    CLI::App* libraryCommand = app.add_subcommand("library", "The games of this account, filtered and sorted as in the window");
    libraryCommand->add_option("search", librarySearch, "A part of the name, or an app id");
    libraryCommand->add_option("--show", libraryShow, "all, cards, installed, played or notplayed");
    libraryCommand->add_option("--sort", librarySort, "name, playtime, last, purchased, reviews, metacritic, released or achievements");
    libraryCommand->add_option("--rating", libraryRating, "At least this many percent positive reviews")->check(CLI::Range(0, 100));
    libraryCommand->callback([&]() { request = { "library", libraryShow, librarySort, std::to_string(libraryRating), librarySearch }; });

    bool serviceStart = false;
    std::string serviceWebAddress;
    CLI::App* service = app.add_subcommand("service", "Run the server as a systemd user service, Linux only");
    service->require_subcommand(1);
    CLI::App* serviceInstall = service->add_subcommand("install", "Install and start it; it starts again at every login");
    serviceInstall->add_flag("--start", serviceStart, "Start the queue as soon as the library is read");
    CLI::Option* serviceWeb = serviceInstall->add_option("--web", serviceWebAddress, "Also serve the web page, at 127.0.0.1:8090 or the address:port given")->expected(0, 1);
    CLI::App* serviceRemove = service->add_subcommand("remove", "Stop and remove it");

    std::string themeFrom;
    std::string themeName;
    CLI::App* themes = app.add_subcommand("themes", "The web page's themes: the built-in ones and those of your own");
    themes->add_option("--themes", themesText, "The folder with themes of your own; Manifold-Idler.themes beside the settings without it");
    themes->require_subcommand(0, 1);
    CLI::App* themesCopy = themes->add_subcommand("copy", "Copy a theme into the themes folder, to change it there");
    themesCopy->add_option("theme", themeFrom, "The theme to copy, arcade for example")->required();
    themesCopy->add_option("name", themeName, "The new theme's name, also its folder's: a to z, 0 to 9 and -")->required();
    themesCopy->add_option("--themes", themesText, "The folder with themes of your own; Manifold-Idler.themes beside the settings without it");

    CLI::App* backupsCommand = app.add_subcommand("backups", "The backups of the settings and statistics");
    CLI::App* backupCommand = app.add_subcommand("backup", "Back up the settings and statistics now");
    std::string restoreName;
    CLI::App* restoreCommand = app.add_subcommand("restore", "Put a backup back; neither the window program nor the server may run");
    restoreCommand->add_option("backup", restoreName, "Its file name, as idler backups lists it")->required();

    std::string explainCode;
    bool explainMarkdown = false;
    CLI::App* explain = app.add_subcommand("explain", "What an error code means, MI-1001 for example; without one, all codes");
    explain->add_option("code", explainCode, "The code, as MI-1001 or 1001");
    explain->add_flag("--markdown", explainMarkdown, "All codes as Markdown, as in Docs/ERRORS.md");

    CLI11_PARSE(app, argc, argv);

    if (explain->parsed())
        return Explain(explainCode, explainMarkdown);

    if (serve->parsed() && serveOptions.Pretend && settingsText.empty())
    {
        std::fprintf(stderr, "--pretend is for checks, with settings of their own.\n");
        return 1;
    }

    std::filesystem::path settings = settingsText.empty() ? Idler::Cli::DefaultSettingsFile() : Idler::Cli::PathFromUtf8(settingsText);
    if (settingsText.empty() && (serve->parsed() || serviceInstall->parsed()))
        settings = Idler::Cli::TakeOverOldSettings(settings);

    if (backupsCommand->parsed())
        return Idler::Cli::ListBackups(settings);
    if (backupCommand->parsed())
        return Idler::Cli::BackUp(settings);
    if (restoreCommand->parsed())
        return Idler::Cli::RestoreBackup(settings, restoreName);

    if (serviceInstall->parsed())
        return Idler::Cli::InstallService(settings, serviceStart, serviceWeb->count() > 0, serviceWebAddress);
    if (serviceRemove->parsed())
        return Idler::Cli::RemoveService(settings);

    const std::filesystem::path themesFolder = themesText.empty() ? Idler::Cli::DefaultThemesFolder(settings) : Idler::Cli::PathFromUtf8(themesText);
    if (themesCopy->parsed())
    {
        std::string text;
        const bool copied = Idler::Cli::CopyTheme(themesFolder, themeFrom, themeName, text);
        std::fprintf(copied ? stdout : stderr, "%s\n", text.c_str());
        return copied ? 0 : 1;
    }
    if (themes->parsed())
    {
        std::fputs(Idler::Cli::ThemesText(Idler::Cli::LoadThemes(themesFolder)).c_str(), stdout);
        return 0;
    }

    if (serve->parsed())
    {
        serveOptions.Settings = settings;
        serveOptions.Web = serveWeb->count() > 0;
        serveOptions.Themes = themesFolder;
        return Idler::Cli::Serve(serveOptions);
    }
    return Ask(settings, request);
}
