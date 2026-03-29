//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <VulkanCommandPool.h>

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanInstance;

    struct VulkanContext : ThreadContext {
        explicit VulkanContext(VulkanDevice& dev) : device(dev) {}

        void OnAttach(uint32_t threadIndex) override;
        void OnDetach() override;

        VulkanDevice& device;
        std::unique_ptr<VulkanCommandPool> pool;
    };

    class VulkanDevice : public Device {
    public:
        explicit VulkanDevice(VulkanInstance &inst);
        ~VulkanDevice() override;

        Fence *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer* CreateBuffer(const Buffer::Descriptor &desc) override { return nullptr; }
        Image* CreateImage(const Image::Descriptor &desc) override { return nullptr; }
        Sampler* CreateSampler(const Sampler::Descriptor &desc) override { return nullptr; }
        ResourceGroup* CreateSampler(const ResourceGroup::Descriptor &desc) override { return nullptr; }
        SwapChain* CreateSwapChain(const SwapChain::Descriptor &desc) override { return nullptr; }

        ShaderFunction* CreateShaderFunction(const ShaderFunction::Descriptor &desc) override { return nullptr; }
        Shader* CreateShader(const Shader::Descriptor &desc) override { return nullptr; }
        GraphicsPipeline* CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override { return nullptr; }
        ComputePipeline* CreatePipelineState(const ComputePipeline::Descriptor &desc) override { return nullptr; }

        VkDevice         GetNativeHandle() const { return device; }
        VkPhysicalDevice GetGpuHandle() const { return gpu; }

        uint32_t GetQueueFamilyIndex(QueueType type) const;
    private:
        ThreadContext* CreateAsyncContext() override;
        bool OnInit(const DeviceInit& init) override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        bool CreateDevice();

        VulkanInstance &instance;

        VkPhysicalDevice gpu    = VK_NULL_HANDLE;
        VkDevice         device = VK_NULL_HANDLE;
        VkQueue          graphicsQueue = VK_NULL_HANDLE;
        VkQueue          computeQueue  = VK_NULL_HANDLE;
        VkQueue          transferQueue = VK_NULL_HANDLE;

        uint32_t graphicsQueueFamily = 0;
        uint32_t computeQueueFamily  = 0;
        uint32_t transferQueueFamily = 0;

        VkPhysicalDeviceProperties   gpuProperties   = {};
        VkPhysicalDeviceFeatures     gpuFeatures     = {};
        VkPhysicalDeviceMemoryProperties memoryProperties = {};

        std::vector<const char*> enabledExtensions;
    };

} // namespace sky::aurora
