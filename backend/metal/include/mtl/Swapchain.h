//
// Created by Zach Lee on 2023/4/8.
//

#pragma once

#include <vector>
#include <rhi/Swapchain.h>
#include <mtl/DevObject.h>
#include <mtl/MetalView.h>
#include <mtl/Image.h>
#import <AppKit/NSWindow.h>
#import <QuartzCore/CAMetalLayer.h>

namespace sky::mtl {
    class Device;

    class SwapChain : public rhi::SwapChain, public DevObject {
    public:
        SwapChain(Device &dev) : DevObject(dev) {}
        ~SwapChain();

        uint32_t AcquireNextImage(const rhi::SemaphorePtr &semaphore) const override;
        rhi::ImagePtr GetImage(uint32_t index) const override;

        rhi::PixelFormat GetFormat() const override { return rhi::PixelFormat::BGRA8_UNORM; }
        const rhi::Extent2D &GetExtent() const override { return extent; };
        uint32_t GetImageCount() const override { return backBufferCount; }
        bool HasDepthStencilImage() const override { return false; }
        rhi::ImagePtr GetDepthStencilImage() const override { return {}; }

        void Resize(uint32_t width, uint32_t height, void* window) override {}
        void Present(rhi::Queue &queue, const rhi::PresentInfo &info) override;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        MetalView* view = nullptr;
        uint32_t currentImageIndex = 0;
        uint32_t backBufferCount = 2;
        rhi::Extent2D extent = {1, 1};

        std::vector<ImagePtr> colorImages;
    };

}

