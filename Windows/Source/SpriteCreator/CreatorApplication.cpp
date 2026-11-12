#include "SpriteCreator/CreatorApplication.h"

#include "App/Desktop.h"
#include "App/WindowCommon.h"
#include "UI/IdleAnimation.h"

namespace Idler
{
    namespace
    {
        using namespace WindowCommon;

        WindowHost::Setup HostSetup()
        {
            WindowHost::Setup setup;
            setup.ClassName = L"ManifoldIdlerSpriteCreator";
            setup.Title = L"Sprite Creator";
            setup.MinimumWidth = SpriteCreator::kMinimumWidth;
            setup.MinimumHeight = SpriteCreator::kMinimumHeight;
            return setup;
        }
    }

    CreatorApplication::CreatorApplication() : m_host(*this, HostSetup())
    {
    }

    bool CreatorApplication::Initialise(HINSTANCE instance, int showCommand)
    {
        if (!m_host.Open(instance))
            return false;

        WindowPlacement placement;
        placement.Width = SpriteCreator::kStartWidth;
        placement.Height = SpriteCreator::kStartHeight;
        m_host.Place(placement);
        ::ShowWindow(m_host.Window(), m_captureFile.empty() ? showCommand : SW_SHOWNOACTIVATE);
        ::UpdateWindow(m_host.Window());

        if (!m_host.StartRendering())
            return false;
        m_host.NoteInput();
        m_running = true;
        return true;
    }

    int CreatorApplication::Run()
    {
        while (m_running)
        {
            if (!m_host.PumpMessages() || m_screen.TakeQuit())
                m_running = false;
            if (!m_running)
                break;

            if (m_host.Hidden())
            {
                m_host.Wait(kHiddenUpdateMilliseconds);
                continue;
            }

            if (m_captureFile.empty() && !UI::IdleAnimation::NeedsFullRate() && m_host.InputAge() > kBusyMilliseconds)
                m_host.Wait(UI::IdleAnimation::NeedsFrames() ? kAnimationFrameMilliseconds : kIdleFrameMilliseconds);

            m_host.ApplyPendingResize();
            m_host.RenderFrame();
        }
        return 0;
    }

    void CreatorApplication::DrawFrame()
    {
        m_screen.Draw(m_host);
    }

    void CreatorApplication::AfterDraw()
    {
        if (m_captureFile.empty() || ++m_captureFrames < kCaptureSettleFrames)
            return;
        ID3D11Texture2D* backBuffer = nullptr;
        if (SUCCEEDED(m_host.SwapChain()->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer)
        {
            Desktop::SavePng(m_host.Device(), m_host.Context(), backBuffer, m_captureFile);
            backBuffer->Release();
        }
        m_running = false;
    }
}
