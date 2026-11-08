#include "Ipc.h"

#include "Paths.h"

#ifdef _WIN32
#include <winsock2.h>
#include <afunix.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#include <cstring>
#include <system_error>

namespace Idler::Cli
{
    namespace
    {
#ifdef _WIN32
        using Socket = SOCKET;
        const Socket kNoSocket = INVALID_SOCKET;
        void CloseSocket(Socket socket) { ::closesocket(socket); }
        constexpr int kSendEnd = SD_SEND;
        constexpr int kSendFlags = 0;

        bool StartSockets()
        {
            static const bool started = []()
                {
                    WSADATA data{};
                    return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
                }();
            return started;
        }

        void SetTimeout(Socket socket, int milliseconds)
        {
            const DWORD value = static_cast<DWORD>(milliseconds);
            ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
            ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
        }
#else
        using Socket = int;
        const Socket kNoSocket = -1;
        void CloseSocket(Socket socket) { ::close(socket); }
        constexpr int kSendEnd = SHUT_WR;
#ifdef MSG_NOSIGNAL
        constexpr int kSendFlags = MSG_NOSIGNAL;
#else
        constexpr int kSendFlags = 0;
#endif

        bool StartSockets()
        {
            return true;
        }

        void SetTimeout(Socket socket, int milliseconds)
        {
            timeval value{};
            value.tv_sec = milliseconds / 1000;
            value.tv_usec = (milliseconds % 1000) * 1000;
            ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
            ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
        }
#endif

        constexpr std::size_t kLongestLine = 1u << 16;

        bool MakeAddress(const std::filesystem::path& file, sockaddr_un& address)
        {
            const std::string text = PathToUtf8(file);
            if (text.size() >= sizeof(address.sun_path))
                return false;
            std::memset(&address, 0, sizeof(address));
            address.sun_family = AF_UNIX;
            std::memcpy(address.sun_path, text.data(), text.size());
            return true;
        }

        bool SendAll(Socket socket, const std::string& text)
        {
            std::size_t done = 0;
            while (done < text.size())
            {
                const auto sent = ::send(socket, text.data() + done, static_cast<int>(text.size() - done), kSendFlags);
                if (sent <= 0)
                    return false;
                done += static_cast<std::size_t>(sent);
            }
            return true;
        }

        void ReadUntilClosed(Socket socket, std::string& text)
        {
            char chunk[4096];
            while (true)
            {
                const auto count = ::recv(socket, chunk, static_cast<int>(sizeof(chunk)), 0);
                if (count <= 0)
                    return;
                text.append(chunk, static_cast<std::size_t>(count));
            }
        }
    }

    Listener::~Listener()
    {
        if (static_cast<Socket>(m_socket) != kNoSocket)
        {
            CloseSocket(static_cast<Socket>(m_socket));
            std::error_code ignored;
            std::filesystem::remove(m_file, ignored);
        }
    }

    bool Listener::Open(const std::filesystem::path& file, std::string& error)
    {
        sockaddr_un address{};
        if (!StartSockets() || !MakeAddress(file, address))
        {
            error = "No socket can be made at " + PathToUtf8(file);
            return false;
        }
        const Socket socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket == kNoSocket)
        {
            error = "This system has no local sockets. Windows needs to be version 10 1803 or newer.";
            return false;
        }

        std::error_code ignored;
        std::filesystem::remove(file, ignored);
#ifndef _WIN32
        const mode_t before = ::umask(0077);
#endif
        const bool bound = ::bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
#ifndef _WIN32
        ::umask(before);
#endif
        if (!bound || ::listen(socket, 8) != 0)
        {
            CloseSocket(socket);
            error = "The socket could not be opened at " + PathToUtf8(file);
            return false;
        }
        m_socket = static_cast<std::uintptr_t>(socket);
        m_file = file;
        return true;
    }

    void Listener::Serve(int milliseconds, const std::function<std::string(const std::string&)>& answer)
    {
        const Socket socket = static_cast<Socket>(m_socket);
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(socket, &readable);
        timeval wait{};
        wait.tv_sec = milliseconds / 1000;
        wait.tv_usec = (milliseconds % 1000) * 1000;
        if (::select(static_cast<int>(socket) + 1, &readable, nullptr, nullptr, &wait) <= 0)
            return;

        const Socket client = ::accept(socket, nullptr, nullptr);
        if (client == kNoSocket)
            return;
        SetTimeout(client, 1000);

        std::string line;
        char chunk[1024];
        while (line.find('\n') == std::string::npos && line.size() < kLongestLine)
        {
            const auto count = ::recv(client, chunk, static_cast<int>(sizeof(chunk)), 0);
            if (count <= 0)
                break;
            line.append(chunk, static_cast<std::size_t>(count));
        }
        const std::size_t end = line.find('\n');
        if (end != std::string::npos)
            SendAll(client, answer(line.substr(0, end)));
        CloseSocket(client);
    }

    bool Ask(const std::filesystem::path& file, const std::string& line, std::string& reply)
    {
        sockaddr_un address{};
        if (!StartSockets() || !MakeAddress(file, address))
            return false;
        const Socket socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket == kNoSocket)
            return false;
        SetTimeout(socket, 10000);
        bool ok = ::connect(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0 && SendAll(socket, line + "\n");
        if (ok)
        {
            ::shutdown(socket, kSendEnd);
            ReadUntilClosed(socket, reply);
            ok = !reply.empty();
        }
        CloseSocket(socket);
        return ok;
    }
}
