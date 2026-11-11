#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Idler::Cli
{
    class Listener
    {
    public:
        Listener() = default;
        ~Listener();

        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;

        bool Open(const std::filesystem::path& file, std::string& error);
        void Serve(int milliseconds, const std::function<std::string(const std::string&)>& answer);
        std::uintptr_t Handle() const { return m_socket; }

    private:
        std::uintptr_t m_socket = ~std::uintptr_t{ 0 };
        std::filesystem::path m_file;
    };

    bool Ask(const std::filesystem::path& file, const std::string& line, std::string& reply);
}
