#include "Steam/LibraryScan.h"

#include "Core/Text.h"
#include "Steam/WorkerProcess.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <utility>

namespace Idler::Steam
{
    namespace
    {
        struct ScanOutput
        {
            std::vector<Ownership> Owned;
            std::uint32_t Account = 0;
            std::uint32_t FreePackageDate = 0;
            std::string Persona;
            std::string Problem;
        };

        ScanOutput AskOwnership(const std::vector<AppInfoEntry>& apps)
        {
            ScanOutput output;
            WorkerProcess worker;
            if (!worker.Start(L"--scan"))
            {
                output.Problem = "The library could not be read.";
                return output;
            }

            std::string request;
            for (const AppInfoEntry& app : apps)
            {
                if (IsLibraryType(app.Type))
                    request += std::to_string(app.AppId) + "\n";
            }
            worker.Write(request);
            worker.CloseInput();

            bool done = false;
            const auto handle = [&](const std::string& line)
                {
                    std::istringstream parts(line);
                    std::string word;
                    parts >> word;
                    if (word == "app")
                    {
                        Ownership own;
                        int installed = 0;
                        parts >> own.AppId >> own.Purchased >> installed;
                        own.Installed = installed != 0;
                        output.Owned.push_back(own);
                    }
                    else if (word == "client")
                    {
                        parts >> output.Account >> output.FreePackageDate;
                    }
                    else if (word == "persona")
                    {
                        output.Persona = std::string(Trim(std::string_view(line).substr(7)));
                    }
                    else if (word == "error")
                    {
                        parts >> word;
                        std::getline(parts, output.Problem);
                        output.Problem = std::string(Trim(output.Problem));
                    }
                    else if (word == "done")
                    {
                        done = true;
                    }
                };

            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
            std::string line;
            while (!done)
            {
                if (worker.ReadLine(line))
                {
                    handle(line);
                    continue;
                }
                if (!worker.Running())
                {
                    while (worker.ReadLine(line))
                        handle(line);
                    break;
                }
                if (std::chrono::steady_clock::now() > deadline)
                {
                    worker.Terminate();
                    output.Problem = "Steam did not answer.";
                    return output;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            if (!done && output.Problem.empty())
                output.Problem = "The library could not be read.";
            return output;
        }
    }

    ScanResult ScanLibrary(const Status& status)
    {
        ScanResult result;
        result.Account = status.Account;

        const std::vector<std::uint8_t> file = ReadAppInfoFile(status);
        const AppInfoResult apps = file.empty() ? AppInfoResult{ {}, 0, "Steam's list of apps could not be read." } : ParseAppInfo(file);
        result.Problem = apps.Error;
        if (!result.Problem.empty())
            return result;

        ScanOutput owned = AskOwnership(apps.Apps);
        result.Problem = std::move(owned.Problem);
        result.Persona = std::move(owned.Persona);
        if (!result.Problem.empty())
            return result;

        Status known = status;
        if (owned.Account != 0)
            known.Account = owned.Account;
        result.Account = known.Account;
        const PlaytimeMap playtimes = ParsePlaytimes(ReadLocalConfigFile(known));
        result.Games = BuildLibrary(apps.Apps, owned.Owned, playtimes, owned.FreePackageDate, ReadAchievementCounts(known, owned.Owned));
        ReadUnlockRules(known, result.Games);
        return result;
    }
}
