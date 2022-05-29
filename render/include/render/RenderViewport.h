//
// Created by Zach Lee on 2022/5/29.
//


#pragma once

#include <vulkan/Swapchain.h>
#include <memory>

namespace sky {

    class RenderViewport {
    public:
        RenderViewport() = default;
        ~RenderViewport() = default;

    private:
        drv::SwapChainPtr swapChain;
    };
    using RDViewportPtr = std::shared_ptr<RenderViewport>;

}