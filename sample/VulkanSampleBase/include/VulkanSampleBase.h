//
// Created by Zach Lee on 2022/10/13.
//

#pragma once

#include <framework/interface/IModule.h>
#include <framework/interface/ISystem.h>
#include <framework/interface/Interface.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <vulkan/Device.h>
#include <vulkan/Instance.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/Semaphore.h>
#include <vulkan/Swapchain.h>
#include <vulkan/ComputePipeline.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky {
    class NativeWindow;

    class VulkanSampleBase : public IWindowEvent {
    public:
        VulkanSampleBase()  = default;
        ~VulkanSampleBase() = default;

        virtual void OnStart();
        virtual void OnStop();
        virtual void OnTick(float delta);

        vk::ShaderPtr LoadShader(VkShaderStageFlagBits stage, const std::string &path);

        bool CheckFeature() const;
    protected:
        void InitRenderPass();
        void ResetFrameBuffer();

        void OnWindowResize(uint32_t width, uint32_t height) override;

        virtual void InitFeature() {}
        vk::Device::Descriptor deviceInfo;
        vk::Instance *instance = nullptr;
        vk::Device *device = nullptr;

        vk::SwapChainPtr        swapChain;
        vk::SemaphorePtr        imageAvailable;
        vk::SemaphorePtr        renderFinish;
        vk::RenderPassPtr       renderPass;

        vk::CommandBufferPtr            commandBuffer;
        vk::Queue                      *graphicsQueue;
        std::vector<vk::FrameBufferPtr> frameBuffers;
        std::vector<vk::ImageViewPtr>   colorViews;

        uint32_t frameIndex = 0;
        uint32_t frame = 0;
    };
}
