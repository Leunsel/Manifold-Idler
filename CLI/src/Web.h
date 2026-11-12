#pragma once

#include "Commands.h"
#include "Http.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Idler::Cli
{
    inline constexpr std::string_view kDefaultWebAddress = "127.0.0.1:8090";
    inline constexpr std::uint16_t kDefaultWebPort = 8090;

    struct WebAddress
    {
        std::string Host;
        std::uint16_t Port = kDefaultWebPort;
        bool Six = false;
    };

    bool ParseWebAddress(std::string_view text, WebAddress& address, std::string& problem);
    bool IsLoopback(const WebAddress& address);
    bool IsWildcard(const WebAddress& address);
    std::string AddressText(const WebAddress& address);
    std::string PageAddress(std::string_view hostAndPort, std::string_view token);

    bool IsIpLiteral(std::string_view host);
    std::string HostName(std::string_view hostHeader);
    std::vector<std::string> ComputerNames();
    bool HostAllowed(std::string_view hostHeader, const std::vector<std::string>& names);
    bool OriginAllowed(const HttpRequest& request);
    bool TokenMatches(std::string_view expected, std::string_view given);
    bool WebAllows(std::string_view command);

    bool RandomHex(std::size_t bytes, std::string& text);
    bool IsUsableToken(std::string_view token);
    bool LoadWebToken(const std::filesystem::path& file, std::string& token, std::string& error);

    struct WebConfig
    {
        std::string Token;
        std::vector<std::string> Names;
    };

    using CommandRunner = std::function<CommandResult(const std::vector<std::string>&)>;

    std::string WebPage(std::string_view nonce);
    std::string_view WebFontFaces();
    std::string ResultJson(const CommandResult& result);
    HttpResponse PlainResponse(int status, std::string_view text);
    HttpResponse AnswerWeb(const HttpRequest& request, const WebConfig& config, const CommandRunner& run);
}
