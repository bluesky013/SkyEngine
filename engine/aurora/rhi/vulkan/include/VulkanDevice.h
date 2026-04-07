//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <VulkanFunctions.h>
#include <VulkanCommandPool.h>
#include <VulkanBuffer.h>
#include <VulkanImage.h>
#include <VulkanSampler.h>
#include <VulkanShader.h>

#include <vk_mem_alloc.h>
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

        Buffer* CreateBuffer(const Buffer::Descriptor &desc) override;
        Image* CreateImage(const Image::Descriptor &desc) override;
        Sampler* CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroup* CreateSampler(const ResourceGroup::Descriptor &desc) override { return nullptr; }
        SwapChain* CreateSwapChain(const SwapChain::Descriptor &desc) override { return nullptr; }

        ShaderFunction* CreateShaderFunction(const ShaderFunction::Descriptor &desc) override;
        Shader* CreateShader(const Shader::Descriptor &desc) override;
        GraphicsPipeline* CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override { return nullptr; }
        ComputePipeline* CreatePipelineState(const ComputePipeline::Descriptor &desc) override { return nullptr; }

        PixelFormatFeatureFlags GetFormatFeatureFlags(PixelFormat format) const override;

        CommandPool* CreateCommandPool(QueueType type) override;

        VkDevice         GetNativeHandle() const { return device; }
        VkPhysicalDevice GetGpuHandle() const { return gpu; }
        VmaAllocator     GetAllocator() const { return allocator; }
        const VulkanDeviceFunctions &GetDeviceFn() const { return deviceFn; }
        const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const { return memoryProperties; }

        uint32_t GetQueueFamilyIndex(QueueType type) const;
    private:
        ThreadContext* CreateAsyncContext() override;
        bool OnInit(const DeviceInit& init) override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        bool CreateDevice();
        bool CreateAllocator();
        void QueryDeviceFeatures();

        VulkanInstance &instance;

        VkPhysicalDevice gpu       = VK_NULL_HANDLE;
        VkDevice         device    = VK_NULL_HANDLE;
        VmaAllocator     allocator = VK_NULL_HANDLE;
        VulkanDeviceFunctions deviceFn = {};
        VkQueue          graphicsQueue = VK_NULL_HANDLE;
        VkQueue          computeQueue  = VK_NULL_HANDLE;
        VkQueue          transferQueue = VK_NULL_HANDLE;

        uint32_t graphicsQueueFamily = 0;
        uint32_t computeQueueFamily  = 0;
        uint32_t transferQueueFamily = 0;

        VkPhysicalDeviceProperties2  gpuProperties   = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

        // features;
        VkPhysicalDeviceFeatures2    gpuFeatures     = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features vkFeature11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features vkFeature12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features vkFeature13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features vkFeature14 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

        // memory properties
        VkPhysicalDeviceMemoryProperties memoryProperties = {};

        std::vector<const char*> enabledExtensions;
    };

} // namespace sky::aurora
