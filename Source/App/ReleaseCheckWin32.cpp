#include "App/ReleaseCheck.h"

#include "Core/Releases.h"
#include "Core/Text.h"
#include "Core/Version.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <string_view>

namespace Idler::ReleaseCheck
{
    namespace
    {
        constexpr wchar_t kHost[] = L"api.github.com";
        constexpr wchar_t kPath[] = L"/repos/Leunsel/Manifold-Idler/releases/latest";
        constexpr wchar_t kHeaders[] = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        constexpr DWORD kTimeoutMilliseconds = 15000;

        struct Handle
        {
            HINTERNET Value = nullptr;
            ~Handle()
            {
                if (Value)
                    ::WinHttpCloseHandle(Value);
            }
        };

        std::string Failure(const char* what)
        {
            char text[96];
            std::snprintf(text, sizeof(text), "%s (Windows error %lu)", what, ::GetLastError());
            return text;
        }
    }

    bool FetchLatest(std::string& body, std::string& problem)
    {
        static_assert(Releases::kLatestAddress == "https://api.github.com/repos/Leunsel/Manifold-Idler/releases/latest");
        const std::wstring agent = L"Manifold-Idler/" + Utf8ToWide(kVersion);

        Handle session;
        session.Value = ::WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session.Value)
        {
            problem = Failure("no internet access was set up");
            return false;
        }
        ::WinHttpSetTimeouts(session.Value, kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds, kTimeoutMilliseconds);

        Handle connection;
        connection.Value = ::WinHttpConnect(session.Value, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection.Value)
        {
            problem = Failure("GitHub could not be reached");
            return false;
        }

        Handle request;
        request.Value = ::WinHttpOpenRequest(connection.Value, L"GET", kPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request.Value)
        {
            problem = Failure("GitHub could not be asked");
            return false;
        }
        DWORD disable = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS;
        ::WinHttpSetOption(request.Value, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));

        if (!::WinHttpSendRequest(request.Value, kHeaders, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !::WinHttpReceiveResponse(request.Value, nullptr))
        {
            problem = Failure("GitHub did not answer");
            return false;
        }

        DWORD status = 0;
        DWORD size = sizeof(status);
        ::WinHttpQueryHeaders(request.Value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status != 200)
        {
            problem = status == 404 ? "GitHub has no release yet" : status == 403 || status == 429 ? "GitHub asks to wait before the next question"
                                                                                                    : "GitHub answered with HTTP " + std::to_string(status);
            return false;
        }

        std::string text;
        char chunk[16384];
        DWORD count = 0;
        while (::WinHttpReadData(request.Value, chunk, sizeof(chunk), &count) && count > 0)
        {
            text.append(chunk, count);
            if (text.size() > kMostBytes)
            {
                problem = "GitHub's answer is too long";
                return false;
            }
        }
        if (text.empty())
        {
            problem = "GitHub's answer is empty";
            return false;
        }
        body = std::move(text);
        return true;
    }
}
