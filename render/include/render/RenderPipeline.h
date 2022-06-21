//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <vulkan/Swapchain.h>
#include <memory>

namespace sky {

    class RenderScene;
    class RenderGraph;

    class RenderPipeline  {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void SetSwapChain(drv::SwapChainPtr swc)
        {
            swapChain = swc;
        }

    protected:
        drv::SwapChainPtr swapChain;
    };
    using RDPipeline = std::shared_ptr<RenderPipeline>;

}