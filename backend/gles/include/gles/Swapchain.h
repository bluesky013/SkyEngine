//
// Created by Zach Lee on 2023/1/31.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/Swapchain.h>
#include <gles/Forward.h>
#include <gles/DevObject.h>
#include <gles/Image.h>
#include <gles/egl/WindowSurface.h>

namespace sky::gles {
    class Device;

    class SwapChain : public rhi::SwapChain, public DevObject {
    public:
        SwapChain(Device &dev) : DevObject(dev) {}
        ~SwapChain();

        bool Init(const Descriptor &desc);

        rhi::PixelFormat GetFormat() const override { return format; }
        const rhi::Extent2D &GetExtent() const override { return surface->GetExtent(); }
        uint32_t GetImageCount() const override { return 1; }
        bool HasDepthStencilImage() const override { return true; }
        rhi::ImagePtr GetDepthStencilImage() const override { return depth; }
        rhi::ImagePtr GetImage(uint32_t index) const override { return color; }
        uint32_t AcquireNextImage(const rhi::SemaphorePtr &semaphore) const override { return 0; }

        void Present(rhi::Queue &queue, const rhi::PresentInfo &info) override;
    private:
        std::shared_ptr<WindowSurface> surface;
        rhi::PixelFormat format = rhi::PixelFormat::RGBA8_UNORM;
        ImagePtr color;
        ImagePtr depth;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}
