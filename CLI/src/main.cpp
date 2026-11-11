#include "Core/Text.h"
#include "Ipc.h"
#include "Paths.h"
#include "Request.h"
#include "Serve.h"
#include "Service.h"
#include "Steam/Worker.h"

#include "CLI11.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <clocale>
#include <csignal>
#endif

#include <cstdio>
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
            std::fprintf(stderr, "No idler runs with these settings. Start one with: idler serve\n");
            return kNoServer;
        }
        bool ok = false;
        std::string text;
        if (!Idler::Cli::ReadReply(reply, ok, text))
        {
            std::fprintf(stderr, "The idler gave an answer that makes no sense.\n");
            return 1;
        }
        if (!text.empty() && text.back() != '\n')
            text.push_back('\n');
        std::fputs(text.c_str(), ok ? stdout : stderr);
        return ok ? 0 : 1;
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

    bool serviceStart = false;
    CLI::App* service = app.add_subcommand("service", "Run the server as a systemd user service, Linux only");
    service->require_subcommand(1);
    CLI::App* serviceInstall = service->add_subcommand("install", "Install and start it; it starts again at every login");
    serviceInstall->add_flag("--start", serviceStart, "Start the queue as soon as the library is read");
    CLI::App* serviceRemove = service->add_subcommand("remove", "Stop and remove it");

    CLI11_PARSE(app, argc, argv);

    const std::filesystem::path settings = settingsText.empty() ? Idler::Cli::DefaultSettingsFile() : Idler::Cli::PathFromUtf8(settingsText);

    if (serviceInstall->parsed())
        return Idler::Cli::InstallService(settings, serviceStart);
    if (serviceRemove->parsed())
        return Idler::Cli::RemoveService(settings);

    if (serve->parsed())
    {
        serveOptions.Settings = settings;
        if (serveOptions.Pretend && settingsText.empty())
        {
            std::fprintf(stderr, "--pretend is for checks, with settings of their own.\n");
            return 1;
        }
        return Idler::Cli::Serve(serveOptions);
    }
    return Ask(settings, request);
}
