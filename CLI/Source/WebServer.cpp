#include "WebServer.h"

#include "Core/Errors.h"
#include "Sockets.h"

#include <algorithm>

namespace Idler::Cli
{
    namespace
    {
        using Net::kNoSocket;
        using Net::Socket;

        constexpr std::size_t kMostClients = 16;
        constexpr auto kClientPatience = std::chrono::seconds(10);
        constexpr int kSendMilliseconds = 2000;

        Socket AsSocket(std::uintptr_t value)
        {
            return static_cast<Socket>(value);
        }

        void Respond(Socket socket, const HttpResponse& response)
        {
            Net::SetBlocking(socket, true);
            Net::SetTimeout(socket, kSendMilliseconds);
            Net::SendAll(socket, ResponseText(response));
            ::shutdown(socket, Net::kSendEnd);
        }
    }

    WebServer::~WebServer()
    {
        for (const Client& client : m_clients)
            Net::CloseSocket(AsSocket(client.Socket));
        if (AsSocket(m_socket) != kNoSocket)
            Net::CloseSocket(AsSocket(m_socket));
    }

    bool WebServer::Open(const WebAddress& address, std::string& error)
    {
        const std::string where = AddressText(address);
        if (!Net::StartSockets())
        {
            error = ErrorMessage(ErrorCode::NoNetworkSockets);
            return false;
        }

        sockaddr_storage storage{};
        Net::AddressLength length = 0;
        int family = AF_INET;
        bool read = false;
        if (address.Six)
        {
            sockaddr_in6* six = reinterpret_cast<sockaddr_in6*>(&storage);
            six->sin6_family = AF_INET6;
            six->sin6_port = htons(address.Port);
            read = ::inet_pton(AF_INET6, address.Host.c_str(), &six->sin6_addr) == 1;
            length = sizeof(sockaddr_in6);
            family = AF_INET6;
        }
        else
        {
            sockaddr_in* four = reinterpret_cast<sockaddr_in*>(&storage);
            four->sin_family = AF_INET;
            four->sin_port = htons(address.Port);
            read = ::inet_pton(AF_INET, address.Host.c_str(), &four->sin_addr) == 1;
            length = sizeof(sockaddr_in);
        }
        if (!read)
        {
            error = ErrorMessage(ErrorCode::NotAnAddress, where);
            return false;
        }

        const Socket socket = ::socket(family, SOCK_STREAM, 0);
        if (socket == kNoSocket)
        {
            error = ErrorMessage(ErrorCode::NoNetworkSocket, where);
            return false;
        }
        Net::KeepFromChildren(socket);
#ifdef _WIN32
        const BOOL exclusive = TRUE;
        ::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
#else
        const int reuse = 1;
        ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
        if (!Net::FitsSelect(socket) || ::bind(socket, reinterpret_cast<const sockaddr*>(&storage), length) != 0 || ::listen(socket, 16) != 0)
        {
            Net::CloseSocket(socket);
            error = ErrorMessage(ErrorCode::PortTaken, where);
            return false;
        }
        Net::SetBlocking(socket, false);
        m_socket = static_cast<std::uintptr_t>(socket);
        return true;
    }

    void WebServer::Serve(int milliseconds, const std::function<HttpResponse(const HttpRequest&)>& answer, std::uintptr_t wakeOn)
    {
        const Socket listener = AsSocket(m_socket);
        if (listener == kNoSocket)
            return;

        const auto now = std::chrono::steady_clock::now();
        for (auto client = m_clients.begin(); client != m_clients.end();)
        {
            if (now - client->Since > kClientPatience)
            {
                Net::CloseSocket(AsSocket(client->Socket));
                client = m_clients.erase(client);
            }
            else
            {
                ++client;
            }
        }

        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(listener, &readable);
        Socket highest = listener;
        const Socket wake = AsSocket(wakeOn);
        if (wake != kNoSocket && Net::FitsSelect(wake))
        {
            FD_SET(wake, &readable);
            highest = std::max(highest, wake);
        }
        for (const Client& client : m_clients)
        {
            FD_SET(AsSocket(client.Socket), &readable);
            highest = std::max(highest, AsSocket(client.Socket));
        }
        timeval wait{};
        wait.tv_sec = milliseconds / 1000;
        wait.tv_usec = (milliseconds % 1000) * 1000;
        if (::select(static_cast<int>(highest) + 1, &readable, nullptr, nullptr, &wait) <= 0)
            return;

        for (std::size_t index = 0; index < m_clients.size();)
        {
            Client& client = m_clients[index];
            if (FD_ISSET(AsSocket(client.Socket), &readable) && !Read(client, answer))
            {
                Net::CloseSocket(AsSocket(client.Socket));
                m_clients.erase(m_clients.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
        if (!FD_ISSET(listener, &readable))
            return;
        for (std::size_t index = Accept(); index < m_clients.size();)
        {
            if (!Read(m_clients[index], answer))
            {
                Net::CloseSocket(AsSocket(m_clients[index].Socket));
                m_clients.erase(m_clients.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
    }

    std::size_t WebServer::Accept()
    {
        const Socket listener = AsSocket(m_socket);
        std::size_t first = m_clients.size();
        for (int accepted = 0; accepted < 8; ++accepted)
        {
            const Socket socket = ::accept(listener, nullptr, nullptr);
            if (socket == kNoSocket)
                break;
            Net::KeepFromChildren(socket);
            if (!Net::FitsSelect(socket))
            {
                Net::CloseSocket(socket);
                continue;
            }
            Net::SetBlocking(socket, false);
            if (m_clients.size() >= kMostClients)
            {
                Net::CloseSocket(AsSocket(m_clients.front().Socket));
                m_clients.erase(m_clients.begin());
                first = first > 0 ? first - 1 : 0;
            }
            Client client;
            client.Socket = static_cast<std::uintptr_t>(socket);
            client.Since = std::chrono::steady_clock::now();
            m_clients.push_back(std::move(client));
        }
        return first;
    }

    bool WebServer::Read(Client& client, const std::function<HttpResponse(const HttpRequest&)>& answer)
    {
        const Socket socket = AsSocket(client.Socket);
        char chunk[4096];
        const auto count = ::recv(socket, chunk, static_cast<int>(sizeof(chunk)), 0);
        if (count < 0 && Net::WouldBlock())
            return true;
        if (count <= 0)
            return false;
        client.Data.append(chunk, static_cast<std::size_t>(count));

        HttpRequest request;
        switch (ParseRequest(client.Data, request))
        {
        case HttpParse::Incomplete:
            return true;
        case HttpParse::Complete:
            Respond(socket, answer(request));
            return false;
        case HttpParse::TooLarge:
            Respond(socket, PlainResponse(413, "The request is too large."));
            return false;
        default:
            Respond(socket, PlainResponse(400, "That is no request the idler understands."));
            return false;
        }
    }
}
