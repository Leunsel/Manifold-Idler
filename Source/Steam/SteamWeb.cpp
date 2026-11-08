#include "Steam/SteamWeb.h"

#include "Core/Cards.h"
#include "Core/Text.h"
#include "Core/WebPolicy.h"

#include <windows.h>
#include <winhttp.h>

#include <utility>

namespace Idler::SteamWeb
{
    namespace
    {
        constexpr wchar_t kProgramAgent[] = L"Manifold-Idler/0.1";

        struct Handle
        {
            HINTERNET Value = nullptr;
            ~Handle()
            {
                if (Value)
                    ::WinHttpCloseHandle(Value);
            }
        };
    }

    bool ReadPicture(std::string_view address, std::string& bytes)
    {
        if (!IsCardPicture(address) && !IsAchievementIconAddress(address))
            return false;
        const std::string_view rest = address.substr(address.find("//") + 2);
        const auto slash = rest.find('/');
        const std::wstring host = Utf8ToWide(rest.substr(0, slash));
        const std::wstring path = Utf8ToWide(rest.substr(slash));

        Handle session;
        session.Value = ::WinHttpOpen(kProgramAgent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session.Value)
            return false;
        ::WinHttpSetTimeouts(session.Value, 15000, 15000, 15000, 30000);

        Handle connection;
        connection.Value = ::WinHttpConnect(session.Value, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection.Value)
            return false;

        Handle request;
        request.Value = ::WinHttpOpenRequest(connection.Value, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request.Value)
            return false;

        DWORD disable = WINHTTP_DISABLE_COOKIES;
        ::WinHttpSetOption(request.Value, WINHTTP_OPTION_DISABLE_FEATURE, &disable, sizeof(disable));

        if (!::WinHttpSendRequest(request.Value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !::WinHttpReceiveResponse(request.Value, nullptr))
            return false;

        DWORD status = 0;
        DWORD size = sizeof(status);
        ::WinHttpQueryHeaders(request.Value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
        if (status != 200)
            return false;

        std::string body;
        char chunk[16384];
        DWORD count = 0;
        while (::WinHttpReadData(request.Value, chunk, sizeof(chunk), &count) && count > 0)
            body.append(chunk, count);
        if (body.empty())
            return false;
        bytes = std::move(body);
        return true;
    }
}
