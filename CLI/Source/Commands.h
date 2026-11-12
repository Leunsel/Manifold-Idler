#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Idler
{
    class IdlerModel;
}

namespace Idler::Cli
{
    struct CommandResult
    {
        bool Ok = true;
        std::string Text;
        bool Quit = false;
        std::vector<std::string> Columns;
        std::vector<std::vector<std::string>> Rows;
    };

    CommandResult RunCommand(IdlerModel& model, const std::vector<std::string>& words);
    bool RunPageCommand(IdlerModel& model, const std::vector<std::string>& words, CommandResult& result);

    CommandResult Done(std::string text);
    CommandResult Failed(std::string text);
    std::string GameName(const IdlerModel& model, std::uint32_t appId);
    std::string GamesText(std::size_t count);
    std::string Padded(std::string text, std::size_t width);
    bool LibraryReady(const IdlerModel& model, std::string& problem);
}
