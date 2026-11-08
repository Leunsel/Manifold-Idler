#pragma once

#include <string>
#include <string_view>

namespace Idler
{
    class WorkerProcess
    {
    public:
        WorkerProcess() = default;
        ~WorkerProcess();

        WorkerProcess(const WorkerProcess&) = delete;
        WorkerProcess& operator=(const WorkerProcess&) = delete;

        bool Start(const std::wstring& arguments);

        bool Write(std::string_view text);
        void CloseInput();

        bool ReadLine(std::string& line);

        bool Running() const;
        void Terminate();

    private:
        void FillBuffer();

#ifdef _WIN32
        void* m_process = nullptr;
        void* m_input = nullptr;
        void* m_output = nullptr;
#else
        int m_process = 0;
        int m_input = -1;
        int m_output = -1;
        mutable bool m_ended = false;
#endif
        std::string m_buffer;
    };
}
