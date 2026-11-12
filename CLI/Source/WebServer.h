#pragma once

#include "Http.h"
#include "Web.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Idler::Cli
{
    class WebServer
    {
    public:
        WebServer() = default;
        ~WebServer();

        WebServer(const WebServer&) = delete;
        WebServer& operator=(const WebServer&) = delete;

        bool Open(const WebAddress& address, std::string& error);
        void Serve(int milliseconds, const std::function<HttpResponse(const HttpRequest&)>& answer, std::uintptr_t wakeOn = ~std::uintptr_t{ 0 });

    private:
        struct Client
        {
            std::uintptr_t Socket = 0;
            std::string Data;
            std::chrono::steady_clock::time_point Since;
        };

        std::size_t Accept();
        bool Read(Client& client, const std::function<HttpResponse(const HttpRequest&)>& answer);

        std::uintptr_t m_socket = ~std::uintptr_t{ 0 };
        std::vector<Client> m_clients;
    };
}
