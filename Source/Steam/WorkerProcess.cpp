#include "Steam/WorkerProcess.h"

#include <windows.h>

#include <filesystem>
#include <vector>

namespace Idler
{
    namespace
    {
        HANDLE WorkerJob()
        {
            static const HANDLE job = []()
                {
                    HANDLE created = ::CreateJobObjectW(nullptr, nullptr);
                    if (created)
                    {
                        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
                        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                        ::SetInformationJobObject(created, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
                    }
                    return created;
                }();
            return job;
        }

        std::wstring ExecutablePath()
        {
            std::wstring path(MAX_PATH, L'\0');
            DWORD length = 0;
            while ((length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))) == path.size())
                path.resize(path.size() * 2);
            path.resize(length);
            return path;
        }
    }

    WorkerProcess::~WorkerProcess()
    {
        CloseInput();
        if (m_output)
            ::CloseHandle(m_output);
        if (m_process)
            ::CloseHandle(m_process);
    }

    bool WorkerProcess::Start(const std::wstring& arguments)
    {
        SECURITY_ATTRIBUTES inherit{ sizeof(inherit), nullptr, TRUE };
        HANDLE inputRead = nullptr;
        HANDLE outputWrite = nullptr;
        if (!::CreatePipe(&inputRead, &m_input, &inherit, 1u << 20))
            return false;
        if (!::CreatePipe(&m_output, &outputWrite, &inherit, 1u << 16))
        {
            ::CloseHandle(inputRead);
            return false;
        }
        ::SetHandleInformation(m_input, HANDLE_FLAG_INHERIT, 0);
        ::SetHandleInformation(m_output, HANDLE_FLAG_INHERIT, 0);

        SIZE_T attributeSize = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
        std::vector<char> attributeMemory(attributeSize);
        auto* attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeMemory.data());
        ::InitializeProcThreadAttributeList(attributes, 1, 0, &attributeSize);
        HANDLE inherited[] = { inputRead, outputWrite };
        ::UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited, sizeof(inherited), nullptr, nullptr);

        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = inputRead;
        startup.StartupInfo.hStdOutput = outputWrite;
        startup.StartupInfo.hStdError = outputWrite;
        startup.lpAttributeList = attributes;

        const std::wstring executable = ExecutablePath();
        const std::wstring folder = std::filesystem::path(executable).parent_path().wstring();
        std::wstring command = L"\"" + executable + L"\" " + arguments;
        PROCESS_INFORMATION process{};
        const BOOL started = ::CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED | CREATE_NO_WINDOW,
            nullptr, folder.c_str(), &startup.StartupInfo, &process);
        ::DeleteProcThreadAttributeList(attributes);
        ::CloseHandle(inputRead);
        ::CloseHandle(outputWrite);
        if (!started)
            return false;

        if (const HANDLE job = WorkerJob())
            ::AssignProcessToJobObject(job, process.hProcess);
        ::ResumeThread(process.hThread);
        ::CloseHandle(process.hThread);
        m_process = process.hProcess;
        return true;
    }

    bool WorkerProcess::Write(std::string_view text)
    {
        DWORD written = 0;
        return m_input && ::WriteFile(m_input, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size();
    }

    void WorkerProcess::CloseInput()
    {
        if (m_input)
        {
            ::CloseHandle(m_input);
            m_input = nullptr;
        }
    }

    void WorkerProcess::FillBuffer()
    {
        if (!m_output)
            return;
        char chunk[4096];
        DWORD available = 0;
        while (::PeekNamedPipe(m_output, nullptr, 0, nullptr, &available, nullptr) && available > 0)
        {
            DWORD count = 0;
            if (!::ReadFile(m_output, chunk, sizeof(chunk), &count, nullptr) || count == 0)
                return;
            m_buffer.append(chunk, count);
        }
    }

    bool WorkerProcess::ReadLine(std::string& line)
    {
        FillBuffer();
        const auto end = m_buffer.find('\n');
        if (end == std::string::npos)
            return false;
        line.assign(m_buffer, 0, end);
        m_buffer.erase(0, end + 1);
        return true;
    }

    bool WorkerProcess::Running() const
    {
        return m_process && ::WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT;
    }

    void WorkerProcess::Terminate()
    {
        if (m_process)
            ::TerminateProcess(m_process, 1);
    }
}
