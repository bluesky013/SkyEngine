//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    class SwapChain : public RefObject {
    public:
        struct Descriptor {
            void       *window          = nullptr;
            uint32_t    width           = 1;
            uint32_t    height          = 1;
            PixelFormat preferredFormat = PixelFormat::BGRA8_UNORM;
            PresentMode preferredMode   = PresentMode::IMMEDIATE;
        };

        SwapChain() = default;
        ~SwapChain() override = default;
    };
} // namespace sky::aurora