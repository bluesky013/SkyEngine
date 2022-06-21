//
// Created by Zach Lee on 2022/5/29.
//


#pragma once

#include <render/RenderScene.h>
#include <vulkan/Swapchain.h>
#include <memory>

namespace sky {

    class RenderViewport {
    public:
        RenderViewport() = default;
        ~RenderViewport();

        struct ViewportInfo {
            void* wHandle = nullptr;
        };

        void SetScene(RDScenePtr scene);

        void Setup(const ViewportInfo& info);

        void Shutdown();

    private:
        RDScenePtr scene;
        drv::SwapChainPtr swapChain;
    };
    using RDViewportPtr = std::unique_ptr<RenderViewport>;

}