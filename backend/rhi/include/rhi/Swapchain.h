//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/Image.h>

namespace sky::rhi {

    class SwapChain {
    public:
        SwapChain()          = default;
        virtual ~SwapChain() = default;

        struct Descriptor {
            void       *window          = nullptr;
            uint32_t    width           = 1;
            uint32_t    height          = 1;
            PixelFormat preferredFormat = PixelFormat::BGRA8_UNORM;
            PresentMode preferredMode   = PresentMode::IMMEDIATE;
        };

        virtual PixelFormat GetFormat() const = 0;
        virtual const Extent2D &GetExtent() const = 0;
        virtual uint32_t AcquireNextImage() const = 0;
        virtual ImagePtr GetImage(uint32_t index) const = 0;
        virtual uint32_t GetImageCount() const = 0;

        virtual bool HasDepthStencilImage() const = 0;
        virtual rhi::ImagePtr GetDepthStencilImage() const = 0;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}
