#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "Steam/WorkerProcess.h"

#include "Core/Text.h"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <vector>

extern char** environ;

namespace Idler
{
    namespace
    {
        std::string ExecutablePath()
        {
            std::error_code ignored;
            return std::filesystem::read_symlink("/proc/self/exe", ignored).string();
        }

        void CloseBoth(int ends[2])
        {
            ::close(ends[0]);
            ::close(ends[1]);
        }
    }

    WorkerProcess::~WorkerProcess()
    {
        CloseInput();
        if (m_output >= 0)
            ::close(m_output);
        if (m_process > 0 && !m_ended)
        {
            int status = 0;
            ::waitpid(m_process, &status, WNOHANG);
        }
    }

    bool WorkerProcess::Start(const std::wstring& arguments)
    {
        int input[2];
        int output[2];
        if (::pipe2(input, O_CLOEXEC) != 0)
            return false;
        if (::pipe2(output, O_CLOEXEC) != 0)
        {
            CloseBoth(input);
            return false;
        }
#ifdef F_SETPIPE_SZ
        ::fcntl(input[1], F_SETPIPE_SZ, 1 << 20);
#endif
        ::fcntl(output[0], F_SETFL, ::fcntl(output[0], F_GETFL) | O_NONBLOCK);

        const std::string executable = ExecutablePath();
        std::vector<std::string> words{ executable };
        std::istringstream parts(WideToUtf8(arguments));
        for (std::string word; parts >> word;)
            words.push_back(word);
        std::vector<char*> argv;
        for (std::string& word : words)
            argv.push_back(word.data());
        argv.push_back(nullptr);

        posix_spawn_file_actions_t actions;
        ::posix_spawn_file_actions_init(&actions);
        ::posix_spawn_file_actions_adddup2(&actions, input[0], STDIN_FILENO);
        ::posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
        ::posix_spawn_file_actions_adddup2(&actions, output[1], STDERR_FILENO);

        pid_t process = 0;
        const int result = ::posix_spawn(&process, executable.c_str(), &actions, nullptr, argv.data(), environ);
        ::posix_spawn_file_actions_destroy(&actions);
        ::close(input[0]);
        ::close(output[1]);
        if (result != 0)
        {
            ::close(input[1]);
            ::close(output[0]);
            return false;
        }

        m_process = process;
        m_input = input[1];
        m_output = output[0];
        m_ended = false;
        return true;
    }

    bool WorkerProcess::Write(std::string_view text)
    {
        if (m_input < 0)
            return false;
        std::size_t done = 0;
        while (done < text.size())
        {
            const auto written = ::write(m_input, text.data() + done, text.size() - done);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
                return false;
            done += static_cast<std::size_t>(written);
        }
        return true;
    }

    void WorkerProcess::CloseInput()
    {
        if (m_input >= 0)
        {
            ::close(m_input);
            m_input = -1;
        }
    }

    void WorkerProcess::FillBuffer()
    {
        if (m_output < 0)
            return;
        char chunk[4096];
        while (true)
        {
            const auto count = ::read(m_output, chunk, sizeof(chunk));
            if (count < 0 && errno == EINTR)
                continue;
            if (count <= 0)
                return;
            m_buffer.append(chunk, static_cast<std::size_t>(count));
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
        if (m_process <= 0 || m_ended)
            return false;
        int status = 0;
        if (::waitpid(m_process, &status, WNOHANG) == 0)
            return true;
        m_ended = true;
        return false;
    }

    void WorkerProcess::Terminate()
    {
        if (m_process > 0 && !m_ended)
            ::kill(m_process, SIGKILL);
    }
}
