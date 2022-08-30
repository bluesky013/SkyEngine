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
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>

namespace sky::render {
    class NativeWindow;

    class FrameGraphSample : public IModule, public IWindowEvent {
    public:
        FrameGraphSample()  = default;
        ~FrameGraphSample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;

        void PrepareFrameGraph(FrameGraph &graph, drv::ImagePtr output);

        void LoadShader(VkShaderStageFlagBits stage, const std::string &path);

    private:
        RDViewportPtr viewport;

        drv::Device      *device = nullptr;
        drv::SemaphorePtr imageAvailable;
        drv::SemaphorePtr renderFinish;
        drv::ImagePtr     msaaColor;
        drv::ImagePtr     depthStencil;

        drv::CommandPoolPtr   commandPool;
        drv::CommandBufferPtr commandBuffer;
        drv::Queue           *graphicsQueue;

        drv::GraphicsPipelinePtr pso;
        drv::PipelineLayoutPtr   pipelineLayout;
        drv::ShaderPtr           vs;
        drv::ShaderPtr           fs;
        drv::VertexInputPtr      vertexInput;
    };

} // namespace sky::render