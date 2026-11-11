#include "App/WebPanel.h"

#include "Core/Text.h"
#include "Core/WebPolicy.h"

#include <cstdio>
#include <utility>

#include <windows.h>
#include <shellapi.h>

#pragma warning(push, 0)
#include <wrl/client.h>
#include <wrl/event.h>
#include <WebView2.h>
#pragma warning(pop)

namespace Idler
{
    namespace
    {
        using Microsoft::WRL::Callback;
        using Microsoft::WRL::ComPtr;

        constexpr wchar_t kLookScript[] = LR"script((function () {
    if (window.top !== window) return;
    var css = '#global_header,.responsive_header,.responsive_page_menu_ctn,.responsive_local_menu_tab,#footer,#footer_spacer{display:none!important}'
        + 'html,body,.responsive_page_frame,.responsive_page_content,.responsive_page_template_content,.page_content{background:#17181c!important}'
        + '.responsive_page_content,.responsive_page_template_content{padding-top:0!important;margin-top:0!important}'
        + '[data-featuretarget="login"]>div,[data-featuretarget="login"]>div>div,[data-featuretarget="login"]>div>div>div{background:none!important;padding-top:0!important;margin-top:0!important;min-height:0!important}'
        + '.page_content{margin-top:0!important;padding-top:8px!important;width:auto!important}'
        + 'body{min-height:0!important}*::-webkit-scrollbar{display:none!important}';
    var add = function () {
        var style = document.createElement('style');
        style.textContent = css;
        (document.head || document.documentElement).appendChild(style);
    };
    if (document.documentElement) add(); else document.addEventListener('DOMContentLoaded', add);
})();)script";

        constexpr wchar_t kWhoScript[] = LR"script((function () {
    var name = document.getElementById('account_pulldown');
    return (typeof g_steamID === 'string' ? g_steamID : '') + '|' + (name ? name.textContent.trim() : '');
})();)script";

        constexpr wchar_t kTextScript[] = L"document.documentElement.outerHTML";

        constexpr ULONGLONG kSettleMilliseconds = 5000;

        std::string TakeString(LPWSTR text)
        {
            std::string result = text ? WideToUtf8(text) : std::string();
            ::CoTaskMemFree(text);
            return result;
        }

        void OpenInBrowser(const std::string& url)
        {
            if (IsHttps(url))
                ::ShellExecuteW(nullptr, L"open", Utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    struct WebPanel::Impl : std::enable_shared_from_this<WebPanel::Impl>
    {
        HWND Parent = nullptr;
        bool Closed = false;
        WebPanel::State Now = WebPanel::State::Starting;
        std::string Problem;
        std::string Address;
        bool Escape = false;
        RECT Bounds{};
        double Zoom = 1.0;
        ComPtr<ICoreWebView2Controller> Controller;
        ComPtr<ICoreWebView2> View;
        WebPanel::PageHandler OnPage;
        WebPanel::PageHandler OnLoad;
        std::wstring PendingUrl;
        std::function<void()> PendingClear;
        bool Visible = false;
        std::shared_ptr<WebPage> Held;
        ULONGLONG HeldAt = 0;

        void Fail(const char* what, HRESULT result)
        {
            char text[160];
            std::snprintf(text, sizeof(text), "%s (0x%08X)", what, static_cast<unsigned>(result));
            Problem = text;
            Now = WebPanel::State::Failed;
            if (OnLoad)
                std::exchange(OnLoad, nullptr)(WebPage{});
            if (PendingClear)
                std::exchange(PendingClear, nullptr)();
        }

        void ClearNow(std::function<void()> done)
        {
            ComPtr<ICoreWebView2_13> view13;
            ComPtr<ICoreWebView2Profile> profile;
            ComPtr<ICoreWebView2Profile2> profile2;
            if (SUCCEEDED(View.As(&view13)) && SUCCEEDED(view13->get_Profile(&profile)) && SUCCEEDED(profile.As(&profile2)) &&
                SUCCEEDED(profile2->ClearBrowsingDataAll(Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>(
                    [self = shared_from_this(), done](HRESULT) -> HRESULT
                    {
                        if (!self->Closed)
                            done();
                        return S_OK;
                    }).Get())))
                return;
            done();
        }

        void Inspect(bool ok)
        {
            auto page = std::make_shared<WebPage>();
            page->Ok = ok;
            LPWSTR uri = nullptr;
            if (SUCCEEDED(View->get_Source(&uri)))
                page->Address = TakeString(uri);

            View->ExecuteScript(kWhoScript, Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                [self = shared_from_this(), page](HRESULT result, LPCWSTR json) -> HRESULT
                {
                    if (self->Closed)
                        return S_OK;
                    if (SUCCEEDED(result) && json)
                    {
                        const PageUser user = ParsePageUser(DecodeJsonString(WideToUtf8(json)));
                        page->SteamId = user.SteamId;
                        page->UserName = user.Name;
                    }
                    if (!self->OnLoad)
                    {
                        if (self->OnPage)
                            self->OnPage(*page);
                        return S_OK;
                    }
                    if (page->Ok && page->SteamId == 0)
                    {
                        self->Held = page;
                        self->HeldAt = ::GetTickCount64();
                        return S_OK;
                    }
                    self->View->ExecuteScript(kTextScript, Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
                        [self, page](HRESULT textResult, LPCWSTR text) -> HRESULT
                        {
                            if (self->Closed)
                                return S_OK;
                            if (SUCCEEDED(textResult) && text)
                                page->Html = DecodeJsonString(WideToUtf8(text));
                            if (self->OnPage)
                                self->OnPage(*page);
                            if (self->OnLoad)
                                std::exchange(self->OnLoad, nullptr)(*page);
                            return S_OK;
                        }).Get());
                    return S_OK;
                }).Get());
        }

        void Setup(const std::wstring& url, bool visible)
        {
            ComPtr<ICoreWebView2Settings> settings;
            if (SUCCEEDED(View->get_Settings(&settings)))
            {
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_AreDefaultContextMenusEnabled(FALSE);
                settings->put_AreDevToolsEnabled(FALSE);
                settings->put_IsZoomControlEnabled(FALSE);
            }
            ComPtr<ICoreWebView2Settings4> settings4;
            if (settings && SUCCEEDED(settings.As(&settings4)))
            {
                settings4->put_IsPasswordAutosaveEnabled(FALSE);
                settings4->put_IsGeneralAutofillEnabled(FALSE);
            }
            ComPtr<ICoreWebView2Controller2> controller2;
            if (SUCCEEDED(Controller.As(&controller2)))
                controller2->put_DefaultBackgroundColor(COREWEBVIEW2_COLOR{ 255, 0x17, 0x18, 0x1C });
            ComPtr<ICoreWebView2_13> view13;
            if (SUCCEEDED(View.As(&view13)))
            {
                ComPtr<ICoreWebView2Profile> profile;
                if (SUCCEEDED(view13->get_Profile(&profile)))
                    profile->put_PreferredColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME_DARK);
            }

            EventRegistrationToken token{};
            View->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
                [self = shared_from_this()](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT
                {
                    LPWSTR uri = nullptr;
                    args->get_Uri(&uri);
                    const std::string address = TakeString(uri);
                    if (IsSteamPage(address))
                    {
                        self->Held.reset();
                        return S_OK;
                    }
                    args->put_Cancel(TRUE);
                    if (self->Visible)
                        OpenInBrowser(address);
                    return S_OK;
                }).Get(), &token);
            View->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT
                {
                    LPWSTR uri = nullptr;
                    args->get_Uri(&uri);
                    OpenInBrowser(TakeString(uri));
                    args->put_Handled(TRUE);
                    return S_OK;
                }).Get(), &token);
            Visible = visible;
            View->add_SourceChanged(Callback<ICoreWebView2SourceChangedEventHandler>(
                [self = shared_from_this()](ICoreWebView2* view, ICoreWebView2SourceChangedEventArgs*) -> HRESULT
                {
                    LPWSTR uri = nullptr;
                    if (!self->Closed && SUCCEEDED(view->get_Source(&uri)))
                        self->Address = TakeString(uri);
                    return S_OK;
                }).Get(), &token);
            View->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
                [self = shared_from_this()](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT
                {
                    BOOL success = FALSE;
                    args->get_IsSuccess(&success);
                    if (!self->Closed)
                        self->Inspect(success != FALSE);
                    return S_OK;
                }).Get(), &token);
            Controller->add_AcceleratorKeyPressed(Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                [self = shared_from_this()](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* args) -> HRESULT
                {
                    UINT key = 0;
                    COREWEBVIEW2_KEY_EVENT_KIND kind{};
                    args->get_VirtualKey(&key);
                    args->get_KeyEventKind(&kind);
                    if (key == VK_ESCAPE && kind == COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN)
                    {
                        self->Escape = true;
                        args->put_Handled(TRUE);
                    }
                    return S_OK;
                }).Get(), &token);

            View->AddScriptToExecuteOnDocumentCreated(kLookScript, nullptr);
            Controller->put_Bounds(Bounds);
            Controller->put_ZoomFactor(Zoom);
            Controller->put_IsVisible(visible ? TRUE : FALSE);
            Now = WebPanel::State::Ready;

            if (PendingClear)
                ClearNow(std::exchange(PendingClear, nullptr));
            const std::wstring first = PendingUrl.empty() ? url : std::exchange(PendingUrl, std::wstring());
            if (!first.empty())
                View->Navigate(first.c_str());
        }
    };

    WebPanel::WebPanel() = default;

    WebPanel::~WebPanel()
    {
        Close();
    }

    void WebPanel::Open(void* parent, const std::filesystem::path& dataFolder, const std::wstring& url, bool visible)
    {
        Close();
        auto impl = std::make_shared<Impl>();
        m_impl = impl;
        impl->Parent = static_cast<HWND>(parent);
        impl->Bounds = RECT{ 0, 0, 960, 600 };

        ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        const HRESULT started = ::CreateCoreWebView2EnvironmentWithOptions(nullptr, dataFolder.c_str(), nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [impl, url, visible](HRESULT result, ICoreWebView2Environment* environment) -> HRESULT
                {
                    if (impl->Closed)
                        return S_OK;
                    if (FAILED(result) || !environment)
                    {
                        impl->Fail("WebView2 did not start.", result);
                        return S_OK;
                    }
                    const HRESULT created = environment->CreateCoreWebView2Controller(impl->Parent, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [impl, url, visible](HRESULT controllerResult, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (impl->Closed)
                            {
                                if (controller)
                                    controller->Close();
                                return S_OK;
                            }
                            if (FAILED(controllerResult) || !controller)
                            {
                                impl->Fail("WebView2 did not open.", controllerResult);
                                return S_OK;
                            }
                            impl->Controller = controller;
                            if (FAILED(controller->get_CoreWebView2(&impl->View)))
                            {
                                impl->Fail("WebView2 has no page.", E_FAIL);
                                return S_OK;
                            }
                            impl->Setup(url, visible);
                            return S_OK;
                        }).Get());
                    if (FAILED(created))
                        impl->Fail("WebView2 did not open.", created);
                    return S_OK;
                }).Get());
        if (started == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
            impl->Fail("Microsoft's WebView2 Runtime is not installed.", started);
        else if (FAILED(started))
            impl->Fail("WebView2 did not start.", started);
    }

    void WebPanel::Close()
    {
        if (!m_impl)
            return;
        m_impl->Closed = true;
        if (m_impl->Controller)
        {
            m_impl->Controller->put_IsVisible(FALSE);
            m_impl->Controller->Close();
        }
        m_impl->View.Reset();
        m_impl->Controller.Reset();
        m_impl->OnPage = nullptr;
        m_impl->OnLoad = nullptr;
        m_impl->Held.reset();
        m_impl.reset();
    }

    void WebPanel::Hide()
    {
        if (!m_impl)
            return;
        m_impl->Visible = false;
        m_impl->OnPage = nullptr;
        if (m_impl->Controller)
            m_impl->Controller->put_IsVisible(FALSE);
    }

    void WebPanel::Tick()
    {
        if (!m_impl || !m_impl->Held || ::GetTickCount64() - m_impl->HeldAt < kSettleMilliseconds)
            return;
        const WebPage page = *m_impl->Held;
        m_impl->Held.reset();
        if (m_impl->OnLoad)
            std::exchange(m_impl->OnLoad, nullptr)(page);
    }

    void WebPanel::SetPageHandler(PageHandler handler)
    {
        if (m_impl)
            m_impl->OnPage = std::move(handler);
    }

    void WebPanel::Load(const std::wstring& url, PageHandler done)
    {
        if (!m_impl || m_impl->Now == State::Failed)
        {
            done(WebPage{});
            return;
        }
        m_impl->Held.reset();
        m_impl->OnLoad = std::move(done);
        if (m_impl->Now == State::Ready)
            m_impl->View->Navigate(url.c_str());
        else
            m_impl->PendingUrl = url;
    }

    void WebPanel::ClearData(std::function<void()> done)
    {
        if (!m_impl || m_impl->Now == State::Failed)
            done();
        else if (m_impl->Now == State::Ready)
            m_impl->ClearNow(std::move(done));
        else
            m_impl->PendingClear = std::move(done);
    }

    void WebPanel::SetBounds(const WebBounds& bounds)
    {
        const RECT rect{ bounds.Left, bounds.Top, bounds.Right, bounds.Bottom };
        if (!m_impl || ::EqualRect(&m_impl->Bounds, &rect))
            return;
        m_impl->Bounds = rect;
        if (m_impl->Controller)
            m_impl->Controller->put_Bounds(rect);
    }

    void WebPanel::SetZoom(double zoom)
    {
        if (!m_impl || m_impl->Zoom == zoom)
            return;
        m_impl->Zoom = zoom;
        if (m_impl->Controller)
            m_impl->Controller->put_ZoomFactor(zoom);
    }

    WebPanel::State WebPanel::CurrentState() const
    {
        return m_impl ? m_impl->Now : State::Closed;
    }

    std::string WebPanel::Error() const
    {
        return m_impl ? m_impl->Problem : std::string();
    }

    std::string WebPanel::Address() const
    {
        return m_impl ? m_impl->Address : std::string();
    }

    bool WebPanel::TakeEscape()
    {
        if (!m_impl || !m_impl->Escape)
            return false;
        m_impl->Escape = false;
        return true;
    }
}
