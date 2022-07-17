//
// Created by Zach Lee on 2022/5/29.
//


#pragma once

#include <render/RenderScene.h>
#include <framework/window/IWindowEvent.h>
#include <vulkan/Swapchain.h>
#include <memory>

namespace sky {

    class RenderViewport : public IWindowEvent {
    public:
        RenderViewport() = default;
        ~RenderViewport();

        struct ViewportInfo {
            void* wHandle = nullptr;
        };

        void SetScene(RDScenePtr scene);

        void Setup(const ViewportInfo& info);

        void Shutdown();

        drv::SwapChainPtr GetSwapChain() const;

        void* GetNativeHandle() const;

    private:
        void OnWindowResize(uint32_t width, uint32_t height) override;

        RDScenePtr scene;
        void* nativeHandle = nullptr;
        drv::SwapChainPtr swapChain;
    };
    using RDViewportPtr = std::unique_ptr<RenderViewport>;

}