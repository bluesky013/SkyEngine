//
// Created by blues on 2026/3/29.
//

#pragma once

#include <VulkanBuffer.h>
#include <VulkanCommandPool.h>
#include <VulkanFunctions.h>
#include <VulkanImage.h>
#include <VulkanQueue.h>
#include <VulkanSampler.h>
#include <VulkanShader.h>
#include <aurora/rhi/Device.h>

#include <array>
#include <memory>
#include <string>
#include <vector>
#include <vk_mem_alloc.h>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanInstance;

    struct VulkanContext : ThreadContext {
        VulkanContext(VulkanDevice &dev, QueueType type, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        void OnAttach(uint32_t threadIndex) override;
        void OnDetach() override;

        VulkanDevice                      &device;
        std::unique_ptr<VulkanCommandPool> pool; // by QueueType
    };

    class VulkanDevice : public Device {
    public:
        explicit VulkanDevice(VulkanInstance &inst);
        ~VulkanDevice() override;

        Fence     *CreateFence(const Fence::Descriptor &desc) override;
        Semaphore *CreateSema(const Semaphore::Descriptor &desc) override;

        Buffer              *CreateBuffer(const Buffer::Descriptor &desc) override;
        Image               *CreateImage(const Image::Descriptor &desc) override;
        Sampler             *CreateSampler(const Sampler::Descriptor &desc) override;
        ResourceGroupLayout *CreateResourceGroupLayout(const ResourceGroupLayout::Descriptor &desc) override;
        ResourceGroup       *CreateResourceGroup(const ResourceGroup::Descriptor &desc) override;
        PipelineLayout      *CreatePipelineLayout(const PipelineLayout::Descriptor &desc) override;
        SwapChain           *CreateSwapChain(const SwapChain::Descriptor &desc) override;

        ShaderFunction   *CreateShaderFunction(const ShaderFunction::Descriptor &desc) override;
        Shader           *CreateShader(const Shader::Descriptor &desc) override;
        GraphicsPipeline *CreatePipelineState(const GraphicsPipeline::Descriptor &desc) override;
        ComputePipeline  *CreatePipelineState(const ComputePipeline::Descriptor &desc) override;

        PixelFormatFeatureFlags GetFormatFeatureFlags(PixelFormat format) const override;

        DeviceFrameContext *CreateFrameContext(const DeviceFrameContextInitInfo &info) override;

        Queue       *GetQueue(QueueType type) override;
        CommandPool *CreateCommandPool(QueueType type) override;

        VkDevice GetNativeHandle() const
        {
            return device;
        }
        VkPhysicalDevice GetGpuHandle() const
        {
            return gpu;
        }
        VmaAllocator GetAllocator() const
        {
            return allocator;
        }
        const VulkanDeviceFunctions &GetDeviceFn() const
        {
            return deviceFn;
        }
        const VkPhysicalDeviceMemoryProperties &GetMemoryProperties() const
        {
            return memoryProperties;
        }
        VulkanInstance &GetVulkanInstance() const
        {
            return instance;
        }

        uint32_t GetQueueFamilyIndex(QueueType type) const;

    private:
        bool        OnInit(const DeviceInit &init) override;
        void        UpdateDeviceCaps() override;
        std::string GetDeviceInfo() const override;
        void        WaitIdle() const override;

        bool CreateDevice();
        bool CreateAllocator();
        void QueryDeviceFeatures();

        VulkanInstance &instance;

        VkPhysicalDevice      gpu       = VK_NULL_HANDLE;
        VkDevice              device    = VK_NULL_HANDLE;
        VmaAllocator          allocator = VK_NULL_HANDLE;
        VulkanDeviceFunctions deviceFn  = {};

        std::array<std::unique_ptr<VulkanQueue>, 3> queues; // by QueueType

        uint32_t graphicsQueueFamily = 0;
        uint32_t computeQueueFamily  = 0;
        uint32_t transferQueueFamily = 0;

        VkPhysicalDeviceProperties2 gpuProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

        // features;
        VkPhysicalDeviceFeatures2        gpuFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        VkPhysicalDeviceVulkan11Features vkFeature11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features vkFeature12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features vkFeature13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features vkFeature14 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};

        // memory properties
        VkPhysicalDeviceMemoryProperties memoryProperties = {};

        std::vector<const char *> enabledExtensions;
    };

} // namespace sky::aurora
