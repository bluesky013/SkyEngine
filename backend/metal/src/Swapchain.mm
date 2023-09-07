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
        [window setContentView: view];
        [view retain];

        drawables.resize(backBufferCount, nil);
        colorImages.resize(backBufferCount, nullptr);
        colorViews.resize(backBufferCount, nullptr);
        for (uint32_t i = 0; i < backBufferCount; ++i) {
            colorImages[i] = std::make_shared<Image>(device);
            colorImages[i]->imageDesc.extent = {extent.width, extent.height, 1};
            colorImages[i]->imageDesc.usage = rhi::ImageUsageFlagBit::RENDER_TARGET;
            colorImages[i]->imageDesc.format = rhi::PixelFormat::BGRA8_UNORM;
            colorImages[i]->swapchain = this;
            colorImages[i]->imageIndex = i;

            colorViews[i] = colorImages[i]->CreateView({});
        }


        return true;
    }

    uint32_t SwapChain::AcquireNextImage(const rhi::SemaphorePtr &semaphore)
    {
        uint32_t index = currentImageIndex;
        currentImageIndex = (currentImageIndex + 1) % backBufferCount;
        return index;
    }

    id<CAMetalDrawable> SwapChain::RequestDrawable(uint32_t index)
    {
        drawables[index] = [view.metalLayer nextDrawable];
        [drawables[index] retain];
        return drawables[index];
    }

    void SwapChain::Present(rhi::Queue &queue, const rhi::PresentInfo &info)
    {
        // do present
        auto &mtlQueue = static_cast<Queue&>(queue);

        @autoreleasepool {
            auto cmd = [mtlQueue.GetNativeHandle() commandBuffer];
            for (auto &signal : info.semaphores) {
                std::static_pointer_cast<Semaphore>(signal)->Wait(cmd);
            }
            [cmd presentDrawable: drawables[info.imageIndex]];
            [cmd commit];
        }
        // release drawable
        [drawables[info.imageIndex] release];
        drawables[info.imageIndex] = nil;
    }


}
