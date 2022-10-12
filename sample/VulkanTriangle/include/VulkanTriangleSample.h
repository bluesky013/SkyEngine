//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <vulkan/Device.h>
#include <vulkan/Driver.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Semaphore.h>
#include <vulkan/Shader.h>
#include <vulkan/Swapchain.h>
#include <vulkan/VertexInput.h>

namespace sky {
    class NativeWindow;

    class VulkanTriangleSample : public IModule, public IWindowEvent {
    public:
        VulkanTriangleSample()  = default;
        ~VulkanTriangleSample() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;

    private:
        void LoadShader(VkShaderStageFlagBits stage, const std::string &path);

        void ResetFrameBuffer();

        drv::Driver *driver = nullptr;
        drv::Device *device = nullptr;

        drv::GraphicsPipelinePtr pso;
        drv::PipelineLayoutPtr   pipelineLayout;
        drv::ShaderPtr           vs;
        drv::ShaderPtr           fs;
        drv::VertexInputPtr      vertexInput;
        drv::SwapChainPtr        swapChain;
        drv::RenderPassPtr       renderPass;
        drv::SemaphorePtr        imageAvailable;
        drv::SemaphorePtr        renderFinish;

        drv::CommandPoolPtr              commandPool;
        drv::CommandBufferPtr            commandBuffer;
        drv::Queue                      *graphicsQueue;
        std::vector<drv::FrameBufferPtr> frameBuffers;
        std::vector<drv::ImageViewPtr>   colorViews;
    };

} // namespace sky