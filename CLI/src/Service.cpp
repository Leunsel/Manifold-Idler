#include "Service.h"

#include "Core/Errors.h"
#include "Ipc.h"
#include "Paths.h"
#include "Request.h"
#include "Web.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <system_error>
#include <vector>

#ifndef _WIN32
#include <cerrno>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace Idler::Cli
{
    std::string QuoteForUnit(std::string_view text)
    {
        std::string quoted = "\"";
        for (const char character : text)
        {
            if (character == '\\' || character == '"')
            {
                quoted.push_back('\\');
                quoted.push_back(character);
            }
            else if (character == '%')
            {
                quoted += "%%";
            }
            else if (character == '$')
            {
                quoted += "$$";
            }
            else
            {
                quoted.push_back(character);
            }
        }
        quoted.push_back('"');
        return quoted;
    }

    std::string ServiceName(const std::filesystem::path& settings, const std::filesystem::path& defaultSettings)
    {
        if (settings.lexically_normal() == defaultSettings.lexically_normal())
            return "manifold-idler";
        std::string name = "manifold-idler-";
        const std::string stem = PathToUtf8(settings.stem());
        for (const char character : stem)
        {
            const bool plain = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
                character == '-' || character == '_';
            name.push_back(plain ? character : '-');
        }
        if (stem.empty())
            name += "settings";
        return name;
    }

    std::string ServiceUnit(const std::filesystem::path& program, const std::filesystem::path& settings, bool start, std::string_view web)
    {
        std::string unit;
        unit += "[Unit]\n";
        unit += "Description=Manifold Idler\n";
        unit += "\n";
        unit += "[Service]\n";
        unit += "ExecStart=" + QuoteForUnit(PathToUtf8(program)) + " --settings " + QuoteForUnit(PathToUtf8(settings)) + " serve" + (start ? " --start" : "") +
            (web.empty() ? std::string() : " --web " + std::string(web)) + "\n";
        unit += "Restart=on-failure\n";
        unit += "RestartSec=10\n";
        unit += "\n";
        unit += "[Install]\n";
        unit += "WantedBy=default.target\n";
        return unit;
    }

#ifdef _WIN32
    int InstallService(const std::filesystem::path&, bool, bool, const std::string&)
    {
        std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::ServiceIsForLinux).c_str());
        return 1;
    }

    int RemoveService(const std::filesystem::path&)
    {
        std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::ServiceIsForLinux).c_str());
        return 1;
    }
#else
    namespace
    {
        int Run(const std::vector<std::string>& words)
        {
            std::vector<char*> argv;
            for (const std::string& word : words)
                argv.push_back(const_cast<char*>(word.c_str()));
            argv.push_back(nullptr);
            pid_t child = 0;
            if (::posix_spawnp(&child, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
                return -1;
            int status = 0;
            while (::waitpid(child, &status, 0) < 0)
            {
                if (errno != EINTR)
                    return -1;
            }
            return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        }

        int Systemctl(std::vector<std::string> words)
        {
            words.insert(words.begin(), { "systemctl", "--user" });
            return Run(words);
        }

        std::filesystem::path UnitFolder()
        {
            if (const char* config = std::getenv("XDG_CONFIG_HOME"); config && config[0])
                return std::filesystem::path(config) / "systemd" / "user";
            const char* home = std::getenv("HOME");
            return std::filesystem::path(home && home[0] ? home : ".") / ".config" / "systemd" / "user";
        }

        std::filesystem::path ThisProgram()
        {
            std::error_code error;
            const std::filesystem::path program = std::filesystem::read_symlink("/proc/self/exe", error);
            return error ? std::filesystem::path() : program;
        }

        std::filesystem::path Absolute(const std::filesystem::path& path)
        {
            std::error_code error;
            const std::filesystem::path absolute = std::filesystem::absolute(path, error);
            return error ? path : absolute.lexically_normal();
        }

        bool ServerAnswers(const std::filesystem::path& settings)
        {
            std::string reply;
            return Ask(SocketFile(settings), JoinRequest({ "status" }), reply);
        }
    }

    int InstallService(const std::filesystem::path& settingsFile, bool start, bool web, const std::string& webAddress)
    {
        std::string webText;
        if (web)
        {
            WebAddress address;
            std::string problem;
            if (!ParseWebAddress(webAddress, address, problem))
            {
                std::fprintf(stderr, "%s\n", problem.c_str());
                return 1;
            }
            webText = AddressText(address);
        }

        const std::filesystem::path settings = Absolute(settingsFile);
        const std::filesystem::path program = ThisProgram();
        if (program.empty())
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::NoProgramPath).c_str());
            return 1;
        }

        const std::string name = ServiceName(settings, DefaultSettingsFile());
        const std::string unitName = name + ".service";
        const bool active = Systemctl({ "is-active", "--quiet", unitName }) == 0;
        if (!active && ServerAnswers(settings))
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::ServerRunsElsewhere).c_str());
            return 1;
        }

        std::error_code error;
        const std::filesystem::path folder = UnitFolder();
        std::filesystem::create_directories(folder, error);
        const std::filesystem::path file = folder / unitName;
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << ServiceUnit(program, settings, start, webText);
            if (!out)
            {
                std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::UnitNotWritten, PathToUtf8(file)).c_str());
                return 1;
            }
        }

        if (Systemctl({ "daemon-reload" }) != 0)
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::SystemctlFailed).c_str());
            return 1;
        }
        if (Systemctl({ "enable", unitName }) != 0 || Systemctl({ active ? "restart" : "start", unitName }) != 0)
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::ServiceNotStarted, unitName).c_str());
            return 1;
        }

        const std::string command = name == "manifold-idler" ? std::string("idler") : "idler --settings \"" + PathToUtf8(settings) + "\"";
        std::printf("%s runs now and starts again at every login.\n", unitName.c_str());
        std::printf("It starts %s: leave the program there, or move it (to ~/.local/bin, say) and install again.\n", PathToUtf8(program).c_str());
        std::printf("Commands work from any terminal, for example: %s status\n", command.c_str());
        std::printf("Its log: journalctl --user -u %s -f\n", name.c_str());
        if (web)
            std::printf("Its web page, with the token: %s web\n", command.c_str());
        std::printf("Without a login session, for example on a server: loginctl enable-linger $USER\n");
        std::printf("Remove it with: %s service remove\n", command.c_str());
        return 0;
    }

    int RemoveService(const std::filesystem::path& settingsFile)
    {
        const std::filesystem::path settings = Absolute(settingsFile);
        const std::string unitName = ServiceName(settings, DefaultSettingsFile()) + ".service";
        const std::filesystem::path file = UnitFolder() / unitName;
        std::error_code error;
        if (!std::filesystem::exists(file, error))
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::NoService).c_str());
            return 1;
        }

        Systemctl({ "disable", "--now", unitName });
        std::filesystem::remove(file, error);
        Systemctl({ "daemon-reload" });
        if (error)
        {
            std::fprintf(stderr, "%s\n", ErrorMessage(ErrorCode::UnitNotRemoved, PathToUtf8(file)).c_str());
            return 1;
        }
        std::printf("%s is removed and stopped. In a terminal the idler still runs with: idler serve\n", unitName.c_str());
        return 0;
    }
#endif
}
