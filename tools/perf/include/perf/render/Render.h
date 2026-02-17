//
// Created by Zach Lee on 2023/4/22.
//

#pragma once

#include <vulkan/Device.h>
#include <vulkan/Instance.h>
#include <vulkan/Swapchain.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/Image.h>
#include <vulkan/Semaphore.h>
#include <vulkan/RenderPass.h>
#include <framework/window/IWindowEvent.h>

namespace sky::perf {

    class Render : public IWindowEvent {
    public:
        Render() = default;
        ~Render() = default;

        void Init();
        void Stop();
        void OnTick();

        vk::Device *device;

        template <typename T>
        void SetEncoder(T &&func) {
            encoder = std::move(func);
        }

        const vk::RenderPassPtr &GetRenderPass() const { return renderPass; }
        const vk::SwapChainPtr &GetSwapchain() const { return swapChain; }
        const vk::DescriptorSetPoolPtr &GetDescriptorSetPool() const { return pool; }
    private:
        void InitRenderPass();
        void InitDescriptorSetPool();
        void ResetFrameBuffer();

        void OnWindowResize(const WindowResizeEvent& event) override;
        vk::Instance *instance;

        vk::SwapChainPtr         swapChain;
        vk::SemaphorePtr         imageAvailable;
        vk::SemaphorePtr         renderFinish;
        vk::RenderPassPtr        renderPass;
        vk::DescriptorSetPoolPtr pool;

        vk::CommandBufferPtr            commandBuffer;
        vk::FencePtr                    fence;
        vk::Queue                      *graphicsQueue;
        std::vector<vk::FrameBufferPtr> frameBuffers;
        std::vector<vk::ImageViewPtr>   colorViews;

        std::function<void(vk::GraphicsEncoder&)> encoder;
    };

}
