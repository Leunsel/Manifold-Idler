#include "App/ReleaseCheck.h"

#include "Core/Releases.h"
#include "Core/Version.h"

#include <dlfcn.h>

#include <mutex>
#include <string_view>

namespace Idler::ReleaseCheck
{
    namespace
    {
        using Easy = void;
        using List = void;
        using InitFn = Easy* (*)();
        using SetOptionFn = int (*)(Easy*, int, ...);
        using PerformFn = int (*)(Easy*);
        using InfoFn = int (*)(Easy*, int, ...);
        using CleanupFn = void (*)(Easy*);
        using ErrorFn = const char* (*)(int);
        using AppendFn = List* (*)(List*, const char*);
        using FreeListFn = void (*)(List*);
        using WriteFn = std::size_t (*)(char*, std::size_t, std::size_t, void*);

        constexpr int kWriteData = 10001;
        constexpr int kUrl = 10002;
        constexpr int kUserAgent = 10018;
        constexpr int kHeaders = 10023;
        constexpr int kWriteFunction = 20011;
        constexpr int kTimeout = 13;
        constexpr int kConnectTimeout = 78;
        constexpr int kNoSignal = 99;
        constexpr int kProtocols = 181;
        constexpr long kHttpsOnly = 2;
        constexpr int kResponseCode = 0x200002;
        constexpr long kTimeoutSeconds = 15;

        struct Curl
        {
            bool Loaded = false;
            InitFn Init = nullptr;
            SetOptionFn SetOption = nullptr;
            PerformFn Perform = nullptr;
            InfoFn Info = nullptr;
            CleanupFn Cleanup = nullptr;
            ErrorFn Error = nullptr;
            AppendFn Append = nullptr;
            FreeListFn FreeList = nullptr;
        };

        template <typename T>
        T Find(void* library, const char* name)
        {
            return reinterpret_cast<T>(::dlsym(library, name));
        }

        const Curl& Library()
        {
            static Curl curl;
            static std::once_flag once;
            std::call_once(once, []()
                {
                    void* library = ::dlopen("libcurl.so.4", RTLD_NOW | RTLD_LOCAL);
                    if (!library)
                        library = ::dlopen("libcurl.so", RTLD_NOW | RTLD_LOCAL);
                    if (!library)
                        return;
                    curl.Init = Find<InitFn>(library, "curl_easy_init");
                    curl.SetOption = Find<SetOptionFn>(library, "curl_easy_setopt");
                    curl.Perform = Find<PerformFn>(library, "curl_easy_perform");
                    curl.Info = Find<InfoFn>(library, "curl_easy_getinfo");
                    curl.Cleanup = Find<CleanupFn>(library, "curl_easy_cleanup");
                    curl.Error = Find<ErrorFn>(library, "curl_easy_strerror");
                    curl.Append = Find<AppendFn>(library, "curl_slist_append");
                    curl.FreeList = Find<FreeListFn>(library, "curl_slist_free_all");
                    curl.Loaded = curl.Init && curl.SetOption && curl.Perform && curl.Info && curl.Cleanup && curl.Error && curl.Append && curl.FreeList;
                });
            return curl;
        }

        struct Received
        {
            std::string Text;
            bool TooLong = false;
        };

        std::size_t Keep(char* data, std::size_t size, std::size_t count, void* where)
        {
            Received& received = *static_cast<Received*>(where);
            const std::size_t bytes = size * count;
            if (received.Text.size() + bytes > kMostBytes)
            {
                received.TooLong = true;
                return 0;
            }
            received.Text.append(data, bytes);
            return bytes;
        }
    }

    bool FetchLatest(std::string& body, std::string& problem)
    {
        const Curl& curl = Library();
        if (!curl.Loaded)
        {
            problem = "libcurl was not found on this computer";
            return false;
        }
        Easy* easy = curl.Init();
        if (!easy)
        {
            problem = "libcurl did not start";
            return false;
        }

        const std::string address(Releases::kLatestAddress);
        const std::string agent = std::string("Manifold-Idler/") + kVersion;
        List* headers = curl.Append(nullptr, "Accept: application/vnd.github+json");
        headers = curl.Append(headers, "X-GitHub-Api-Version: 2022-11-28");
        Received received;
        const WriteFn keep = Keep;
        curl.SetOption(easy, kUrl, address.c_str());
        curl.SetOption(easy, kUserAgent, agent.c_str());
        curl.SetOption(easy, kHeaders, headers);
        curl.SetOption(easy, kWriteFunction, keep);
        curl.SetOption(easy, kWriteData, &received);
        curl.SetOption(easy, kTimeout, kTimeoutSeconds);
        curl.SetOption(easy, kConnectTimeout, kTimeoutSeconds);
        curl.SetOption(easy, kNoSignal, 1L);
        curl.SetOption(easy, kProtocols, kHttpsOnly);

        const int result = curl.Perform(easy);
        long status = 0;
        curl.Info(easy, kResponseCode, &status);
        curl.Cleanup(easy);
        curl.FreeList(headers);

        if (received.TooLong)
        {
            problem = "GitHub's answer is too long";
            return false;
        }
        if (result != 0)
        {
            problem = std::string("GitHub did not answer: ") + curl.Error(result);
            return false;
        }
        if (status != 200)
        {
            problem = status == 404 ? "GitHub has no release yet" : status == 403 || status == 429 ? "GitHub asks to wait before the next question"
                                                                                                    : "GitHub answered with HTTP " + std::to_string(status);
            return false;
        }
        if (received.Text.empty())
        {
            problem = "GitHub's answer is empty";
            return false;
        }
        body = std::move(received.Text);
        return true;
    }
}
