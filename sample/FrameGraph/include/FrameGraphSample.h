//
// Created by Zach Lee on 2022/6/16.
//


#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/NativeWindow.h>
#include <render/Render.h>
#include <render/RenderScene.h>
#include <render/RenderViewport.h>
#include <render/framegraph/FrameGraph.h>
#include <vulkan/CommandPool.h>
#include <vulkan/CommandBuffer.h>

namespace sky::render {
    class NativeWindow;

    class FrameGraphSample : public IModule, public IWindowEvent {
    public:
        FrameGraphSample() = default;
        ~FrameGraphSample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

    private:
        RDViewportPtr viewport;
        FrameGraph graph;

        drv::Device* device = nullptr;
        drv::SemaphorePtr imageAvailable;
        drv::SemaphorePtr renderFinish;
        drv::ImagePtr msaaColor;
        drv::ImagePtr depthStencil;
        drv::ImagePtr shadowMap;

        drv::CommandPoolPtr commandPool;
        drv::CommandBufferPtr commandBuffer;
        drv::Queue* graphicsQueue;
    };

}