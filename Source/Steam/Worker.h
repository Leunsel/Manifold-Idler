#pragma once

namespace Idler::Worker
{
    bool IsWorkerCommand(const wchar_t* commandLine);
    int Run(const wchar_t* commandLine);
}
