#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#endif

#include <cstddef>
#include <string_view>

namespace Idler::Cli::Net
{
#ifdef _WIN32
    using Socket = SOCKET;
    using AddressLength = int;
    inline const Socket kNoSocket = INVALID_SOCKET;
    inline constexpr int kSendEnd = SD_SEND;
    inline constexpr int kSendFlags = 0;

    inline void CloseSocket(Socket socket)
    {
        ::closesocket(socket);
    }

    inline bool StartSockets()
    {
        static const bool started = []()
            {
                WSADATA data{};
                return ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
            }();
        return started;
    }

    inline void SetTimeout(Socket socket, int milliseconds)
    {
        const DWORD value = static_cast<DWORD>(milliseconds);
        ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
        ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&value), sizeof(value));
    }

    inline void SetBlocking(Socket socket, bool blocking)
    {
        u_long value = blocking ? 0 : 1;
        ::ioctlsocket(socket, FIONBIO, &value);
    }

    inline bool FitsSelect(Socket)
    {
        return true;
    }

    inline bool WouldBlock()
    {
        return ::WSAGetLastError() == WSAEWOULDBLOCK;
    }

    inline void KeepFromChildren(Socket socket)
    {
        ::SetHandleInformation(reinterpret_cast<HANDLE>(socket), HANDLE_FLAG_INHERIT, 0);
    }
#else
    using Socket = int;
    using AddressLength = socklen_t;
    inline constexpr Socket kNoSocket = -1;
    inline constexpr int kSendEnd = SHUT_WR;
#ifdef MSG_NOSIGNAL
    inline constexpr int kSendFlags = MSG_NOSIGNAL;
#else
    inline constexpr int kSendFlags = 0;
#endif

    inline void CloseSocket(Socket socket)
    {
        ::close(socket);
    }

    inline bool StartSockets()
    {
        return true;
    }

    inline void SetTimeout(Socket socket, int milliseconds)
    {
        timeval value{};
        value.tv_sec = milliseconds / 1000;
        value.tv_usec = (milliseconds % 1000) * 1000;
        ::setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
        ::setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
    }

    inline void SetBlocking(Socket socket, bool blocking)
    {
        const int flags = ::fcntl(socket, F_GETFL, 0);
        if (flags >= 0)
            ::fcntl(socket, F_SETFL, blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
    }

    inline bool FitsSelect(Socket socket)
    {
        return socket >= 0 && socket < FD_SETSIZE;
    }

    inline bool WouldBlock()
    {
        return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
    }

    inline void KeepFromChildren(Socket socket)
    {
        const int flags = ::fcntl(socket, F_GETFD, 0);
        if (flags >= 0)
            ::fcntl(socket, F_SETFD, flags | FD_CLOEXEC);
    }
#endif

    inline bool SendAll(Socket socket, std::string_view text)
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
}
