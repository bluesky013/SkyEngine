//
// Created by Zach Lee on 2023/4/8.
//

#pragma once

#include <rhi/Swapchain.h>
#include <mtl/DevObject.h>

#include <AppKit/NSView.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

namespace sky::mtl {
    class Device;

    class SwapChain : public rhi::SwapChain, public DevObject {
    public:
        SwapChain(Device &dev) : DevObject(dev) {}
        ~SwapChain() = default;

        rhi::PixelFormat GetFormat() const override;
        const rhi::Extent2D &GetExtent() const override;
        uint32_t AcquireNextImage(const rhi::SemaphorePtr &semaphore) const override;
        rhi::ImagePtr GetImage(uint32_t index) const override;
        uint32_t GetImageCount() const override;
        bool HasDepthStencilImage() const override;
        rhi::ImagePtr GetDepthStencilImage() const override;
        void Present(rhi::Queue &queue, const rhi::PresentInfo &info) override;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        NS::View* view = nullptr;
    };

}

