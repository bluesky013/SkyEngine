//
// Created by Zach Lee on 2022/5/29.
//

#pragma once

#include <framework/window/IWindowEvent.h>
#include <memory>
#include <render/RenderScene.h>
#include <vulkan/Swapchain.h>

namespace sky {

    class RenderViewport : public IWindowEvent {
    public:
        RenderViewport() = default;
        ~RenderViewport();

        struct ViewportInfo {
            void *wHandle = nullptr;
        };

        void Setup(const ViewportInfo &info);

        void SetScene(const RDScenePtr &scn);

        void Shutdown();

        drv::SwapChainPtr GetSwapChain() const;

        void *GetNativeHandle() const;

        void DoFrame();

        const VkExtent2D &GetExtent() const;

    private:
        void BeginFrame();

        void EndFrame();

        void OnWindowResize(uint32_t width, uint32_t height) override;

        void                 *nativeHandle      = nullptr;
        uint32_t              currentFrame      = 0;
        uint32_t              currentImageIndex = 0;
        RDScenePtr            scene;
        drv::SemaphorePtr     imageAvailable[INFLIGHT_FRAME];
        drv::SemaphorePtr     renderFinish[INFLIGHT_FRAME];
        drv::CommandBufferPtr commandBuffer[INFLIGHT_FRAME];
        drv::CommandPoolPtr   commandPool;
        drv::SwapChainPtr     swapChain;
        drv::Queue           *graphicsQueue{nullptr};
    };
    using RDViewportPtr = std::shared_ptr<RenderViewport>;
} // namespace sky