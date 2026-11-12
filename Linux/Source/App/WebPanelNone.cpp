#include "App/WebPanel.h"

#if defined(IDLER_CARD_FARM)
#error "The card farm needs WebView2, which only the Windows program has"
#endif

namespace Idler
{
    struct WebPanel::Impl
    {
    };

    WebPanel::WebPanel() = default;

    WebPanel::~WebPanel() = default;

    void WebPanel::Open(void*, const std::filesystem::path&, const std::wstring&, bool)
    {
    }

    void WebPanel::Close()
    {
    }

    void WebPanel::Hide()
    {
    }

    void WebPanel::Tick()
    {
    }

    void WebPanel::SetPageHandler(PageHandler)
    {
    }

    void WebPanel::Load(const std::wstring&, PageHandler done)
    {
        if (done)
            done(WebPage{});
    }

    void WebPanel::ClearData(std::function<void()> done)
    {
        if (done)
            done();
    }

    void WebPanel::SetBounds(const WebBounds&)
    {
    }

    void WebPanel::SetZoom(double)
    {
    }

    WebPanel::State WebPanel::CurrentState() const
    {
        return State::Failed;
    }

    std::string WebPanel::Error() const
    {
        return "Signing in to Steam is only in the Windows program.";
    }

    std::string WebPanel::Address() const
    {
        return {};
    }

    bool WebPanel::TakeEscape()
    {
        return false;
    }
}
