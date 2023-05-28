//
// Created by Zach Lee on 2023/4/8.
//

#include <mtl/Swapchain.h>
#include <mtl/MetalView.h>
#include <mtl/Device.h>
#import <Metal/Metal.h>

namespace sky::mtl {

    SwapChain::~SwapChain()
    {
        [view release];
        view = nullptr;
    }

    bool SwapChain::Init(const Descriptor &desc)
    {
        auto window = static_cast<NSWindow *>(desc.window);

        extent.width = static_cast<uint32_t>(window.frame.size.width);
        extent.height = static_cast<uint32_t>(window.frame.size.height);

        view = [[MetalView alloc] initWithFrame:window device:device.GetMetalDevice()];
        [view retain];

        colorImages.resize(backBufferCount);
        for (uint32_t i = 0; i < backBufferCount; ++i) {
            colorImages[i] = std::make_shared<Image>(device);
            colorImages[i]->imageDesc.extent = {extent.width, extent.height, 1};
            colorImages[i]->imageDesc.usage = rhi::ImageUsageFlagBit::RENDER_TARGET;
            colorImages[i]->imageDesc.format = rhi::PixelFormat::RGBA8_UNORM;
        }

        return true;
    }

    uint32_t SwapChain::AcquireNextImage(const rhi::SemaphorePtr &semaphore) const
    {
        auto drawable = [view.metalLayer nextDrawable];
        colorImages[currentImageIndex]->SetDrawable(drawable);
        return currentImageIndex;
    }

    rhi::ImagePtr SwapChain::GetImage(uint32_t index) const
    {
        return colorImages[index];
    }

    void SwapChain::Present(rhi::Queue &queue, const rhi::PresentInfo &info)
    {
        // do present

        // release drawable
        colorImages[currentImageIndex]->ResetDrawable();
        currentImageIndex = (currentImageIndex + 1) % backBufferCount;
    }


}
