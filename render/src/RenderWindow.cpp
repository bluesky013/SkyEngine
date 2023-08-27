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

        swapChain = RHI::Get()->GetDevice()->CreateSwapChain(desc);
        return static_cast<bool>(swapChain);
    }

    void RenderWindow::Resize(void *hWnd, uint32_t width, uint32_t height)
    {
        RHI::Get()->GetDevice()->WaitIdle();
        swapChain->Resize(width, height, hWnd);
    }

    uint32_t RenderWindow::GetWidth() const
    {
        return swapChain->GetExtent().width;
    }

    uint32_t RenderWindow::GetHeight() const
    {
        return swapChain->GetExtent().height;
    }

    rhi::PixelFormat RenderWindow::GetOutputFormat() const
    {
        return swapChain->GetFormat();
    }
} // namespace sky