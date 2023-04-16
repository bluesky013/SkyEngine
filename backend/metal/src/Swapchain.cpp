//
// Created by Zach Lee on 2023/4/8.
//

#include <mtl/Swapchain.h>

namespace sky::mtl {

    bool SwapChain::Init(const Descriptor &desc)
    {
        view = static_cast<NS::View*>(desc.window);
        return true;
    }

    rhi::PixelFormat SwapChain::GetFormat() const
    {
        return {};
    }

    const rhi::Extent2D &SwapChain::GetExtent() const
    {
    }

    uint32_t SwapChain::AcquireNextImage(const rhi::SemaphorePtr &semaphore) const
    {
        return 0;
    }

    rhi::ImagePtr SwapChain::GetImage(uint32_t index) const
    {
        return {};
    }

    uint32_t SwapChain::GetImageCount() const
    {
        return 0;
    }

    bool SwapChain::HasDepthStencilImage() const
    {
        return false;
    }

    rhi::ImagePtr SwapChain::GetDepthStencilImage() const
    {
        return {};
    }

    void SwapChain::Present(rhi::Queue &queue, const rhi::PresentInfo &info)
    {
    }


}
