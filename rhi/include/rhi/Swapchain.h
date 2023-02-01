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

        virtual bool Init(const Descriptor &desc) = 0;

    protected:
        Descriptor descriptor;
    };
    using SwapChainPtr = std::shared_ptr<SwapChain>;
}
