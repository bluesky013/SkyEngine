//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <vulkan/Swapchain.h>
#include <list>

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

        virtual void Render(RenderScene& scene, RenderGraph&) = 0;

    protected:
        drv::SwapChainPtr swapChain;
    };

}