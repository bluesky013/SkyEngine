//
// Created by Zach Lee on 2023/8/17.
//

#include <render/RenderWindow.h>

#include <render/RHI.h>

namespace sky {

    bool RenderWindow::Init(void *hWnd, uint32_t width, uint32_t height, bool vSync)
    {
        rhi::SwapChain::Descriptor desc = {};
        desc.window = hWnd;
        desc.width = width;
        desc.height = height;
        desc.preferredMode = vSync ? rhi::PresentMode::VSYNC : rhi::PresentMode::IMMEDIATE;
        winHandle = hWnd;

        swapChain = RHI::Get()->GetDevice()->CreateSwapChain(desc);
        return static_cast<bool>(swapChain);
    }

#ifdef SKY_ENABLE_XR
    bool RenderWindow::InitByXR(const rhi::XRSwapChainPtr &xr)
    {
        xrSwapChain = xr;
        return static_cast<bool>(xrSwapChain);
    }
#endif

    void RenderWindow::Resize(uint32_t width, uint32_t height)
    {
        RHI::Get()->GetDevice()->WaitIdle();

        if (swapChain) {
            swapChain->Resize(width, height, winHandle);
        }
    }

    uint32_t RenderWindow::GetWidth() const
    {
#ifdef SKY_ENABLE_XR
        return swapChain ? swapChain->GetExtent().width : xrSwapChain->GetExtent().width;
#elif
        return swapChain->GetExent().width;
#endif
    }

    uint32_t RenderWindow::GetHeight() const
    {
#ifdef SKY_ENABLE_XR
        return swapChain ? swapChain->GetExtent().height : xrSwapChain->GetExtent().height;
#elif
        return swapChain->GetExent().height;
#endif
    }
} // namespace sky