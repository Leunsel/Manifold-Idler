#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Idler::Cli
{
    inline constexpr std::size_t kLongestHead = 16 * 1024;
    inline constexpr std::size_t kLongestBody = 64 * 1024;
    inline constexpr std::size_t kMostWords = 256;

    struct HttpRequest
    {
        std::string Method;
        std::string Path;
        std::string Query;
        std::vector<std::pair<std::string, std::string>> Headers;
        std::string Body;

        const std::string* Header(std::string_view name) const;
    };

    enum class HttpParse
    {
        Incomplete,
        Complete,
        Bad,
        TooLarge
    };

    HttpParse ParseRequest(std::string_view data, HttpRequest& request);
    std::string QueryValue(std::string_view query, std::string_view name);

    struct HttpResponse
    {
        int Status = 200;
        std::string Type;
        std::string Body;
        std::vector<std::pair<std::string, std::string>> Headers;

        const std::string* Header(std::string_view name) const;
    };

    std::string ResponseText(const HttpResponse& response);

    std::string JsonString(std::string_view text);
    bool ParseJsonWords(std::string_view json, std::vector<std::string>& words);
}
