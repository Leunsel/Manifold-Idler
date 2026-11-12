#include "Web.h"

#include "Core/Errors.h"
#include "Core/Text.h"
#include "Paths.h"
#include "Sockets.h"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <system_error>

namespace Idler::Cli
{
    namespace
    {
        constexpr std::string_view kLockedPolicy = "default-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
        constexpr std::string_view kWebCommands[] = { "ping", "status", "queue", "add", "remove", "goal", "games", "lists", "use", "log", "start", "stop", "slots",
            "move", "sort", "retry", "now", "refund", "clear", "undo", "list", "rescan", "library", "game", "stats", "history", "settings", "set" };
        constexpr std::string_view kCoverPath = "/cover/";
        constexpr std::string_view kThemesPath = "/themes/";
        constexpr std::uintmax_t kLargestPicture = 8u * 1024u * 1024u;

        std::string LowerAscii(std::string_view text)
        {
            std::string out(text);
            for (char& character : out)
                character = character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
            return out;
        }

        bool AllDigits(std::string_view text)
        {
            if (text.empty())
                return false;
            for (const char character : text)
            {
                if (character < '0' || character > '9')
                    return false;
            }
            return true;
        }

        bool ReadV4(std::string_view text, in_addr& address)
        {
            Net::StartSockets();
            const std::string copy(text);
            return ::inet_pton(AF_INET, copy.c_str(), &address) == 1;
        }

        bool ReadV6(std::string_view text, in6_addr& address)
        {
            Net::StartSockets();
            const std::string copy(text);
            return ::inet_pton(AF_INET6, copy.c_str(), &address) == 1;
        }

        void AddSafetyHeaders(HttpResponse& response, std::string policy)
        {
            response.Headers.emplace_back("Content-Security-Policy", std::move(policy));
            response.Headers.emplace_back("Cache-Control", "no-store");
            response.Headers.emplace_back("X-Content-Type-Options", "nosniff");
            response.Headers.emplace_back("X-Frame-Options", "DENY");
            response.Headers.emplace_back("Referrer-Policy", "no-referrer");
            response.Headers.emplace_back("Cross-Origin-Opener-Policy", "same-origin");
            response.Headers.emplace_back("Cross-Origin-Resource-Policy", "same-origin");
        }

        HttpResponse JsonResponse(int status, std::string body)
        {
            HttpResponse response;
            response.Status = status;
            response.Type = "application/json; charset=utf-8";
            response.Body = std::move(body);
            AddSafetyHeaders(response, std::string(kLockedPolicy));
            return response;
        }

        std::string CodeField(std::string_view text)
        {
            const ErrorInfo* info = ErrorOf(text);
            return info ? ",\"code\":" + JsonString(ErrorName(info->Code)) : std::string();
        }

        HttpResponse JsonProblem(int status, std::string_view text)
        {
            return JsonResponse(status, "{\"ok\":false,\"text\":" + JsonString(text) + CodeField(text) + "}");
        }

        HttpResponse WrongMethod(std::string_view allowed)
        {
            HttpResponse response = PlainResponse(405, "Not with this method.");
            response.Headers.emplace_back("Allow", std::string(allowed));
            return response;
        }

        bool StartsWithIgnoreCase(std::string_view text, std::string_view start)
        {
            return text.size() >= start.size() && EqualsIgnoreCase(text.substr(0, start.size()), start);
        }

        bool WriteToken(const std::filesystem::path& file, const std::string& token)
        {
            const std::string text = token + "\n";
#ifdef _WIN32
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            out << text;
            return static_cast<bool>(out);
#else
            const int handle = ::open(PathToUtf8(file).c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
            if (handle < 0)
                return false;
            ::fchmod(handle, 0600);
            const bool written = ::write(handle, text.data(), text.size()) == static_cast<ssize_t>(text.size());
            return ::close(handle) == 0 && written;
#endif
        }
    }

    bool ParseWebAddress(std::string_view text, WebAddress& address, std::string& problem)
    {
        if (text.empty())
            text = kDefaultWebAddress;
        const std::string given(text);
        const std::string example = " Give one like 127.0.0.1:8090, 0.0.0.0:8090 or [::1]:8090.";

        std::string_view host;
        std::string_view port;
        bool six = false;
        if (AllDigits(text))
        {
            host = "127.0.0.1";
            port = text;
        }
        else if (text.front() == '[')
        {
            const std::size_t close = text.find(']');
            if (close == std::string_view::npos)
            {
                problem = "Not an address: " + given + "." + example;
                return false;
            }
            host = text.substr(1, close - 1);
            const std::string_view rest = text.substr(close + 1);
            if (!rest.empty() && rest.front() != ':')
            {
                problem = "Not an address: " + given + "." + example;
                return false;
            }
            port = rest.empty() ? std::string_view() : rest.substr(1);
            six = true;
        }
        else
        {
            const std::size_t colon = text.find(':');
            if (colon != std::string_view::npos && text.find(':', colon + 1) != std::string_view::npos)
            {
                problem = "An IPv6 address goes in brackets: [" + given + "]:8090.";
                return false;
            }
            host = text.substr(0, colon);
            port = colon == std::string_view::npos ? std::string_view() : text.substr(colon + 1);
            if (colon != std::string_view::npos && port.empty())
            {
                problem = "No port after the colon: " + given + "." + example;
                return false;
            }
        }

        if (EqualsIgnoreCase(host, "localhost"))
            host = "127.0.0.1";
        in_addr four{};
        in6_addr sixAddress{};
        if (six ? !ReadV6(host, sixAddress) : !ReadV4(host, four))
        {
            problem = "Not an address: " + given + ". Names other than localhost are not taken, only numbers." + example;
            return false;
        }

        std::uint32_t number = kDefaultWebPort;
        if (!port.empty())
        {
            const auto parsed = AllDigits(port) ? ParseUnsigned(port) : std::nullopt;
            if (!parsed || *parsed == 0 || *parsed > 65535)
            {
                problem = "Not a port: " + std::string(port) + ". A port is from 1 to 65535.";
                return false;
            }
            number = *parsed;
        }

        address.Host = std::string(host);
        address.Port = static_cast<std::uint16_t>(number);
        address.Six = six;
        return true;
    }

    bool IsLoopback(const WebAddress& address)
    {
        if (!address.Six)
        {
            in_addr four{};
            return ReadV4(address.Host, four) && reinterpret_cast<const unsigned char*>(&four)[0] == 127;
        }
        in6_addr six{};
        if (!ReadV6(address.Host, six))
            return false;
        const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&six);
        static constexpr unsigned char kLoopback[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
        static constexpr unsigned char kMapped[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
        return std::memcmp(bytes, kLoopback, 16) == 0 || (std::memcmp(bytes, kMapped, 12) == 0 && bytes[12] == 127);
    }

    bool IsWildcard(const WebAddress& address)
    {
        if (!address.Six)
        {
            in_addr four{};
            return ReadV4(address.Host, four) && std::memcmp(&four, "\0\0\0\0", 4) == 0;
        }
        in6_addr six{};
        static constexpr unsigned char kAny[16] = {};
        return ReadV6(address.Host, six) && std::memcmp(&six, kAny, 16) == 0;
    }

    std::string AddressText(const WebAddress& address)
    {
        return (address.Six ? "[" + address.Host + "]" : address.Host) + ":" + std::to_string(address.Port);
    }

    std::string PageAddress(std::string_view hostAndPort, std::string_view token)
    {
        return "http://" + std::string(hostAndPort) + "/?token=" + std::string(token);
    }

    bool IsIpLiteral(std::string_view host)
    {
        if (host.size() >= 2 && host.front() == '[' && host.back() == ']')
        {
            in6_addr six{};
            return ReadV6(host.substr(1, host.size() - 2), six);
        }
        in_addr four{};
        return ReadV4(host, four);
    }

    std::string HostName(std::string_view hostHeader)
    {
        std::string_view name;
        std::string_view rest;
        if (!hostHeader.empty() && hostHeader.front() == '[')
        {
            const std::size_t close = hostHeader.find(']');
            if (close == std::string_view::npos)
                return {};
            name = hostHeader.substr(0, close + 1);
            rest = hostHeader.substr(close + 1);
        }
        else
        {
            const std::size_t colon = hostHeader.find(':');
            name = hostHeader.substr(0, colon);
            rest = colon == std::string_view::npos ? std::string_view() : hostHeader.substr(colon);
        }
        if (!rest.empty() && (rest.front() != ':' || !AllDigits(rest.substr(1))))
            return {};
        return LowerAscii(name);
    }

    std::vector<std::string> ComputerNames()
    {
        std::vector<std::string> names;
        if (!Net::StartSockets())
            return names;
        char text[256] = {};
        if (::gethostname(text, static_cast<int>(sizeof(text) - 1)) != 0 || text[0] == '\0')
            return names;
        const std::string name = LowerAscii(text);
        names.push_back(name);
        if (name.find('.') == std::string::npos)
            names.push_back(name + ".local");
        return names;
    }

    bool HostAllowed(std::string_view hostHeader, const std::vector<std::string>& names)
    {
        const std::string name = HostName(hostHeader);
        if (name.empty())
            return false;
        if (name == "localhost" || IsIpLiteral(name))
            return true;
        for (const std::string& allowed : names)
        {
            if (name == LowerAscii(allowed))
                return true;
        }
        return false;
    }

    bool OriginAllowed(const HttpRequest& request)
    {
        if (const std::string* site = request.Header("sec-fetch-site"); site && *site != "same-origin" && *site != "none")
            return false;
        const std::string* origin = request.Header("origin");
        if (!origin)
            return true;
        const std::string* host = request.Header("host");
        return host && EqualsIgnoreCase(*origin, "http://" + *host);
    }

    bool TokenMatches(std::string_view expected, std::string_view given)
    {
        if (expected.empty() || expected.size() != given.size())
            return false;
        unsigned char difference = 0;
        for (std::size_t index = 0; index < expected.size(); ++index)
            difference |= static_cast<unsigned char>(expected[index] ^ given[index]);
        return difference == 0;
    }

    bool WebAllows(std::string_view command)
    {
        for (const std::string_view allowed : kWebCommands)
        {
            if (command == allowed)
                return true;
        }
        return false;
    }

    bool RandomHex(std::size_t bytes, std::string& text)
    {
        std::string raw(bytes, '\0');
#ifdef _WIN32
        if (!BCRYPT_SUCCESS(::BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(raw.data()), static_cast<ULONG>(raw.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
            return false;
#else
        std::size_t done = 0;
        while (done < raw.size())
        {
            const ssize_t got = ::getrandom(raw.data() + done, raw.size() - done, 0);
            if (got < 0)
            {
                if (errno == EINTR)
                    continue;
                return false;
            }
            done += static_cast<std::size_t>(got);
        }
#endif
        static constexpr char kDigits[] = "0123456789abcdef";
        text.clear();
        for (const char byte : raw)
        {
            const auto value = static_cast<unsigned char>(byte);
            text.push_back(kDigits[value >> 4]);
            text.push_back(kDigits[value & 0x0F]);
        }
        return true;
    }

    bool IsUsableToken(std::string_view token)
    {
        if (token.size() < 16 || token.size() > 256)
            return false;
        for (const char character : token)
        {
            const bool plain = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') ||
                character == '-' || character == '_';
            if (!plain)
                return false;
        }
        return true;
    }

    bool LoadWebToken(const std::filesystem::path& file, std::string& token, std::string& error)
    {
        std::error_code missing;
        if (std::filesystem::exists(file, missing))
        {
            std::ifstream in(file, std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            const std::string_view kept = Trim(text);
            if (!kept.empty())
            {
                if (!IsUsableToken(kept))
                {
                    error = ErrorMessage(ErrorCode::TokenUnusable, PathToUtf8(file));
                    return false;
                }
                token = std::string(kept);
                return true;
            }
        }
        if (!RandomHex(16, token))
        {
            error = ErrorMessage(ErrorCode::NoRandomToken);
            return false;
        }
        if (!WriteToken(file, token))
        {
            error = ErrorMessage(ErrorCode::TokenNotSaved, PathToUtf8(file));
            return false;
        }
        return true;
    }

    std::string ResultJson(const CommandResult& result)
    {
        std::string json = std::string("{\"ok\":") + (result.Ok ? "true" : "false") + ",\"text\":" + JsonString(result.Text) + CodeField(result.Text);
        if (!result.Columns.empty())
        {
            json += ",\"columns\":[";
            for (std::size_t index = 0; index < result.Columns.size(); ++index)
                json += (index > 0 ? "," : "") + JsonString(result.Columns[index]);
            json += "],\"rows\":[";
            for (std::size_t row = 0; row < result.Rows.size(); ++row)
            {
                json += row > 0 ? ",[" : "[";
                for (std::size_t index = 0; index < result.Rows[row].size(); ++index)
                    json += (index > 0 ? "," : "") + JsonString(result.Rows[row][index]);
                json += "]";
            }
            json += "]";
        }
        json += "}";
        return json;
    }

    HttpResponse PlainResponse(int status, std::string_view text)
    {
        HttpResponse response;
        response.Status = status;
        response.Type = "text/plain; charset=utf-8";
        response.Body = std::string(text) + "\n";
        AddSafetyHeaders(response, std::string(kLockedPolicy));
        return response;
    }

    HttpResponse PictureResponse(const std::filesystem::path& file)
    {
        std::error_code error;
        const std::string extension = LowerAscii(PathToUtf8(file.extension()));
        const char* type = extension == ".jpg" || extension == ".jpeg" ? "image/jpeg" : extension == ".png" ? "image/png" : nullptr;
        const std::uintmax_t size = file.empty() ? 0 : std::filesystem::file_size(file, error);
        if (!type || error || size == 0 || size > kLargestPicture)
            return PlainResponse(404, "No picture.");
        std::ifstream in(file, std::ios::binary);
        std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (body.size() != size)
            return PlainResponse(404, "No picture.");
        HttpResponse response;
        response.Type = type;
        response.Body = std::move(body);
        response.Headers.emplace_back("Content-Security-Policy", std::string(kLockedPolicy));
        response.Headers.emplace_back("Cache-Control", "private, max-age=86400");
        response.Headers.emplace_back("X-Content-Type-Options", "nosniff");
        response.Headers.emplace_back("Referrer-Policy", "no-referrer");
        response.Headers.emplace_back("Cross-Origin-Resource-Policy", "same-origin");
        return response;
    }

    HttpResponse ThemeResponse(const std::filesystem::path& folder, std::string_view id, std::string_view file)
    {
        HttpResponse response;
        if (!FindThemeFile(folder, id, file, response.Body, response.Type))
            return PlainResponse(404, "No such theme file.");
        response.Headers.emplace_back("Content-Security-Policy", std::string(kLockedPolicy));
        response.Headers.emplace_back("Cache-Control", "no-cache");
        response.Headers.emplace_back("X-Content-Type-Options", "nosniff");
        response.Headers.emplace_back("Referrer-Policy", "no-referrer");
        response.Headers.emplace_back("Cross-Origin-Resource-Policy", "same-origin");
        return response;
    }

    HttpResponse AnswerWeb(const HttpRequest& request, const WebConfig& config, const CommandRunner& run)
    {
        const std::string* host = request.Header("host");
        if (!host || !HostAllowed(*host, config.Names))
            return PlainResponse(403, ErrorMessage(ErrorCode::WrongHost));
        if (!OriginAllowed(request))
            return PlainResponse(403, ErrorMessage(ErrorCode::WrongOrigin));
        if (request.Method != "GET" && request.Method != "POST")
            return WrongMethod("GET, POST");

        const std::string_view path = request.Path;
        const bool themeFile = path.starts_with(kThemesPath);
        std::string_view themePart;
        std::string given;
        if (themeFile)
        {
            const std::string_view rest = path.substr(kThemesPath.size());
            const std::size_t slash = rest.find('/');
            given = std::string(rest.substr(0, slash));
            themePart = slash == std::string_view::npos ? std::string_view() : rest.substr(slash + 1);
        }
        else
        {
            const std::string* header = request.Header("x-idler-token");
            given = header ? *header : QueryValue(request.Query, "token");
        }
        if (!TokenMatches(config.Token, given))
            return PlainResponse(401, ErrorMessage(ErrorCode::WrongToken));

        if (themeFile)
        {
            if (request.Method != "GET")
                return WrongMethod("GET");
            const std::size_t slash = themePart.find('/');
            if (slash == std::string_view::npos)
                return PlainResponse(404, "No such theme file.");
            return ThemeResponse(config.Themes, themePart.substr(0, slash), themePart.substr(slash + 1));
        }

        if (request.Path == "/")
        {
            if (request.Method != "GET")
                return WrongMethod("GET");
            std::string nonce;
            if (!RandomHex(16, nonce))
                return PlainResponse(500, ErrorMessage(ErrorCode::NoRandomNonce));
            HttpResponse response;
            response.Type = "text/html; charset=utf-8";
            response.Body = WebPage(nonce, LoadThemes(config.Themes));
            AddSafetyHeaders(response, "default-src 'none'; script-src 'nonce-" + nonce + "'; style-src 'nonce-" + nonce +
                "' 'self'; connect-src 'self'; img-src data: 'self'; font-src data: 'self'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'");
            return response;
        }

        if (request.Path == "/api")
        {
            if (request.Method != "POST")
                return WrongMethod("POST");
            const std::string* type = request.Header("content-type");
            if (!type || !StartsWithIgnoreCase(*type, "application/json"))
                return JsonProblem(415, ErrorMessage(ErrorCode::NotJson));
            std::vector<std::string> words;
            if (!ParseJsonWords(request.Body, words) || words.empty())
                return JsonProblem(400, ErrorMessage(ErrorCode::NotWords));
            if (!WebAllows(words[0]))
                return JsonProblem(403, ErrorMessage(ErrorCode::NotFromPage, words[0]));
            return JsonResponse(200, ResultJson(run(words)));
        }

        if (std::string_view(request.Path).starts_with(kCoverPath))
        {
            if (request.Method != "GET")
                return WrongMethod("GET");
            const auto appId = ParseUnsigned(std::string_view(request.Path).substr(kCoverPath.size()));
            return PictureResponse(appId && config.Cover ? config.Cover(*appId) : std::filesystem::path());
        }

        return PlainResponse(404, "Nothing here.");
    }
}
