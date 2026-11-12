#pragma once

#include <cwchar>
#include <string>

namespace Idler::CommandLine
{
    inline std::wstring Argument(const wchar_t* commandLine, const wchar_t* name)
    {
        const wchar_t* found = commandLine ? std::wcsstr(commandLine, name) : nullptr;
        if (!found)
            return {};
        found += std::wcslen(name);
        std::wstring value;
        if (*found == L'"')
        {
            for (++found; *found && *found != L'"'; ++found)
                value.push_back(*found);
        }
        else
        {
            for (; *found && *found != L' '; ++found)
                value.push_back(*found);
        }
        return value;
    }

    inline bool Has(const wchar_t* commandLine, const wchar_t* flag)
    {
        return commandLine && std::wcsstr(commandLine, flag);
    }
}
