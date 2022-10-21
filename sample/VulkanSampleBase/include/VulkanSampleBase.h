//
// Created by Zach Lee on 2022/10/13.
//

#pragma once

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <vulkan/Device.h>
#include <vulkan/Driver.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/Semaphore.h>
#include <vulkan/Swapchain.h>
#include <vulkan/ComputePipeline.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky {
    class NativeWindow;

    class VulkanSampleBase : public IModule, public IWindowEvent {
    public:
        VulkanSampleBase()  = default;
        ~VulkanSampleBase() = default;

        void Init() override;

        void Start() override;

        void Stop() override;

        void Tick(float delta) override;

        void OnWindowResize(uint32_t width, uint32_t height) override;

        virtual void OnStart() {}
        virtual void OnStop() {}

        drv::ShaderPtr LoadShader(VkShaderStageFlagBits stage, const std::string &path);

    protected:
        void ResetFrameBuffer();

        drv::Driver *driver = nullptr;
        drv::Device *device = nullptr;

        drv::SwapChainPtr        swapChain;
        drv::SemaphorePtr        imageAvailable;
        drv::SemaphorePtr        renderFinish;
        drv::RenderPassPtr       renderPass;

        drv::CommandPoolPtr              commandPool;
        drv::CommandBufferPtr            commandBuffer;
        drv::Queue                      *graphicsQueue;
        std::vector<drv::FrameBufferPtr> frameBuffers;
        std::vector<drv::ImageViewPtr>   colorViews;

        uint32_t frameIndex = 0;
        uint32_t frame = 0;
    };
}
