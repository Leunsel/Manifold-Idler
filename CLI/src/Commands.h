#pragma once

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
}
