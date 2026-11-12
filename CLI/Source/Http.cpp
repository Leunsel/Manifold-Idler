#include "Http.h"

#include "Core/Text.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace Idler::Cli
{
    namespace
    {
        bool IsTokenCharacter(char character)
        {
            if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))
                return true;
            return std::string_view("!#$%&'*+-.^_`|~").find(character) != std::string_view::npos;
        }

        char Lower(char character)
        {
            return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
        }

        std::string_view TrimSpace(std::string_view text)
        {
            while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
                text.remove_prefix(1);
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
                text.remove_suffix(1);
            return text;
        }

        int HexDigit(char character)
        {
            if (character >= '0' && character <= '9')
                return character - '0';
            if (character >= 'a' && character <= 'f')
                return character - 'a' + 10;
            if (character >= 'A' && character <= 'F')
                return character - 'A' + 10;
            return -1;
        }

        std::string PercentDecoded(std::string_view text)
        {
            std::string out;
            out.reserve(text.size());
            for (std::size_t at = 0; at < text.size(); ++at)
            {
                const char character = text[at];
                if (character == '+')
                {
                    out.push_back(' ');
                }
                else if (character == '%' && at + 2 < text.size() && HexDigit(text[at + 1]) >= 0 && HexDigit(text[at + 2]) >= 0)
                {
                    out.push_back(static_cast<char>(HexDigit(text[at + 1]) * 16 + HexDigit(text[at + 2])));
                    at += 2;
                }
                else
                {
                    out.push_back(character);
                }
            }
            return out;
        }

        const char* Reason(int status)
        {
            switch (status)
            {
            case 200: return "OK";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 413: return "Content Too Large";
            case 415: return "Unsupported Media Type";
            case 501: return "Not Implemented";
            default: return "Internal Server Error";
            }
        }

        void AppendUtf8(std::string& out, std::uint32_t code)
        {
            if (code < 0x80)
            {
                out.push_back(static_cast<char>(code));
            }
            else if (code < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (code >> 6)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else if (code < 0x10000)
            {
                out.push_back(static_cast<char>(0xE0 | (code >> 12)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xF0 | (code >> 18)));
                out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
            }
        }

        bool ReadHex4(std::string_view json, std::size_t at, std::uint32_t& value)
        {
            if (at + 4 > json.size())
                return false;
            value = 0;
            for (std::size_t index = at; index < at + 4; ++index)
            {
                const int digit = HexDigit(json[index]);
                if (digit < 0)
                    return false;
                value = value * 16 + static_cast<std::uint32_t>(digit);
            }
            return true;
        }

        bool ReadJsonString(std::string_view json, std::size_t& at, std::string& out)
        {
            if (at >= json.size() || json[at] != '"')
                return false;
            ++at;
            while (at < json.size())
            {
                const char character = json[at++];
                if (character == '"')
                    return true;
                if (static_cast<unsigned char>(character) < 0x20)
                    return false;
                if (character != '\\')
                {
                    out.push_back(character);
                    continue;
                }
                if (at >= json.size())
                    return false;
                switch (json[at++])
                {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    std::uint32_t code = 0;
                    if (!ReadHex4(json, at, code))
                        return false;
                    at += 4;
                    if (code >= 0xDC00 && code <= 0xDFFF)
                        return false;
                    if (code >= 0xD800 && code <= 0xDBFF)
                    {
                        std::uint32_t low = 0;
                        if (at + 1 >= json.size() || json[at] != '\\' || json[at + 1] != 'u' || !ReadHex4(json, at + 2, low) || low < 0xDC00 || low > 0xDFFF)
                            return false;
                        at += 6;
                        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                    }
                    AppendUtf8(out, code);
                    break;
                }
                default:
                    return false;
                }
            }
            return false;
        }
    }

    const std::string* HttpRequest::Header(std::string_view name) const
    {
        for (const auto& [key, value] : Headers)
        {
            if (key == name)
                return &value;
        }
        return nullptr;
    }

    const std::string* HttpResponse::Header(std::string_view name) const
    {
        for (const auto& [key, value] : Headers)
        {
            if (EqualsIgnoreCase(key, name))
                return &value;
        }
        return nullptr;
    }

    HttpParse ParseRequest(std::string_view data, HttpRequest& request)
    {
        const std::size_t headEnd = data.find("\r\n\r\n");
        if (headEnd == std::string_view::npos)
            return data.size() > kLongestHead ? HttpParse::TooLarge : HttpParse::Incomplete;
        if (headEnd > kLongestHead)
            return HttpParse::TooLarge;

        request = {};
        const std::string_view head = data.substr(0, headEnd);
        const std::size_t firstEnd = head.find("\r\n");
        const std::string_view line = head.substr(0, firstEnd);

        const std::size_t methodEnd = line.find(' ');
        const std::size_t targetEnd = methodEnd == std::string_view::npos ? std::string_view::npos : line.find(' ', methodEnd + 1);
        if (targetEnd == std::string_view::npos || line.find(' ', targetEnd + 1) != std::string_view::npos)
            return HttpParse::Bad;
        const std::string_view method = line.substr(0, methodEnd);
        const std::string_view target = line.substr(methodEnd + 1, targetEnd - methodEnd - 1);
        const std::string_view version = line.substr(targetEnd + 1);
        if (method.empty() || (version != "HTTP/1.1" && version != "HTTP/1.0"))
            return HttpParse::Bad;
        for (const char character : method)
        {
            if (character < 'A' || character > 'Z')
                return HttpParse::Bad;
        }
        if (target.empty() || target.front() != '/')
            return HttpParse::Bad;
        for (const char character : target)
        {
            const auto code = static_cast<unsigned char>(character);
            if (code <= 0x20 || code >= 0x7F)
                return HttpParse::Bad;
        }

        request.Method = std::string(method);
        const std::size_t question = target.find('?');
        request.Path = std::string(target.substr(0, question));
        if (question != std::string_view::npos)
            request.Query = std::string(target.substr(question + 1));

        std::size_t at = firstEnd == std::string_view::npos ? head.size() : firstEnd + 2;
        while (at < head.size())
        {
            const std::size_t end = std::min(head.find("\r\n", at), head.size());
            const std::string_view field = head.substr(at, end - at);
            at = end + 2;
            if (field.empty() || field.front() == ' ' || field.front() == '\t')
                return HttpParse::Bad;
            const std::size_t colon = field.find(':');
            if (colon == std::string_view::npos || colon == 0)
                return HttpParse::Bad;
            std::string name;
            for (const char character : field.substr(0, colon))
            {
                if (!IsTokenCharacter(character))
                    return HttpParse::Bad;
                name.push_back(Lower(character));
            }
            const std::string_view value = TrimSpace(field.substr(colon + 1));
            for (const char character : value)
            {
                const auto code = static_cast<unsigned char>(character);
                if ((code < 0x20 && code != '\t') || code == 0x7F)
                    return HttpParse::Bad;
            }
            if ((name == "host" || name == "content-length" || name == "origin" || name == "x-idler-token") && request.Header(name))
                return HttpParse::Bad;
            request.Headers.emplace_back(std::move(name), std::string(value));
        }

        if (request.Header("transfer-encoding"))
            return HttpParse::Bad;
        std::size_t length = 0;
        if (const std::string* text = request.Header("content-length"))
        {
            if (text->empty() || text->size() > 9)
                return HttpParse::Bad;
            for (const char character : *text)
            {
                if (character < '0' || character > '9')
                    return HttpParse::Bad;
                length = length * 10 + static_cast<std::size_t>(character - '0');
            }
            if (length > kLongestBody)
                return HttpParse::TooLarge;
        }
        const std::size_t bodyStart = headEnd + 4;
        if (data.size() - bodyStart < length)
            return HttpParse::Incomplete;
        request.Body = std::string(data.substr(bodyStart, length));
        return HttpParse::Complete;
    }

    std::string QueryValue(std::string_view query, std::string_view name)
    {
        while (!query.empty())
        {
            const std::size_t end = query.find('&');
            const std::string_view pair = query.substr(0, end);
            query = end == std::string_view::npos ? std::string_view() : query.substr(end + 1);
            const std::size_t equals = pair.find('=');
            if (PercentDecoded(pair.substr(0, equals)) == name)
                return equals == std::string_view::npos ? std::string() : PercentDecoded(pair.substr(equals + 1));
        }
        return {};
    }

    std::string ResponseText(const HttpResponse& response)
    {
        std::string text = "HTTP/1.1 " + std::to_string(response.Status) + " " + Reason(response.Status) + "\r\n";
        if (!response.Type.empty())
            text += "Content-Type: " + response.Type + "\r\n";
        text += "Content-Length: " + std::to_string(response.Body.size()) + "\r\n";
        for (const auto& [name, value] : response.Headers)
            text += name + ": " + value + "\r\n";
        text += "Connection: close\r\n\r\n";
        text += response.Body;
        return text;
    }

    std::string JsonString(std::string_view text)
    {
        std::string out = "\"";
        out.reserve(text.size() + 2);
        for (const char character : text)
        {
            switch (character)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(character)));
                    out += escaped;
                }
                else
                {
                    out.push_back(character);
                }
            }
        }
        out.push_back('"');
        return out;
    }

    bool ParseJsonWords(std::string_view json, std::vector<std::string>& words)
    {
        words.clear();
        std::size_t at = 0;
        const auto skip = [&]()
            {
                while (at < json.size() && (json[at] == ' ' || json[at] == '\t' || json[at] == '\n' || json[at] == '\r'))
                    ++at;
            };
        skip();
        if (at >= json.size() || json[at] != '[')
            return false;
        ++at;
        skip();
        if (at < json.size() && json[at] == ']')
        {
            ++at;
            skip();
            return at == json.size();
        }
        while (words.size() < kMostWords)
        {
            std::string word;
            if (!ReadJsonString(json, at, word))
                return false;
            words.push_back(std::move(word));
            skip();
            if (at >= json.size())
                return false;
            if (json[at] == ']')
            {
                ++at;
                skip();
                return at == json.size();
            }
            if (json[at] != ',')
                return false;
            ++at;
            skip();
        }
        return false;
    }
}
