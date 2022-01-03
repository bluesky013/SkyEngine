//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <vulkan/Swapchain.h>
#include <list>

namespace sky {

    class RenderGraph;

    class RenderPipeline  {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        void SetSwapChain(drv::SwapChainPtr swc)
        {
            swapChain = swc;
            Setup();
        }

        virtual void Render(RenderGraph&) = 0;

    protected:
        virtual void Setup() = 0;

        drv::SwapChainPtr swapChain;
    };

}