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
    };

    CommandResult RunCommand(IdlerModel& model, const std::vector<std::string>& words);
}
