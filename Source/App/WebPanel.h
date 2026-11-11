#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace Idler
{
    struct WebBounds
    {
        long Left = 0;
        long Top = 0;
        long Right = 0;
        long Bottom = 0;

        bool operator==(const WebBounds&) const = default;
    };

    struct WebPage
    {
        bool Ok = false;
        std::string Address;
        std::uint64_t SteamId = 0;
        std::string UserName;
        std::string Html;
    };

    class WebPanel
    {
    public:
        enum class State
        {
            Closed,
            Starting,
            Ready,
            Failed
        };

        using PageHandler = std::function<void(const WebPage&)>;

        WebPanel();
        ~WebPanel();

        WebPanel(const WebPanel&) = delete;
        WebPanel& operator=(const WebPanel&) = delete;

        void Open(void* parent, const std::filesystem::path& dataFolder, const std::wstring& url, bool visible);
        void Close();
        void Hide();
        void Tick();

        void SetPageHandler(PageHandler handler);
        void Load(const std::wstring& url, PageHandler done);
        void ClearData(std::function<void()> done);

        void SetBounds(const WebBounds& bounds);
        void SetZoom(double zoom);

        State CurrentState() const;
        bool IsOpen() const { return m_impl != nullptr; }
        std::string Error() const;
        std::string Address() const;
        bool TakeEscape();

    private:
        struct Impl;
        std::shared_ptr<Impl> m_impl;
    };
}
