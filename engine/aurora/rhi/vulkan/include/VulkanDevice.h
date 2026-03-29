//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Device.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

namespace sky::aurora {

    class VulkanInstance;

    class VulkanDevice : public Device {
    public:
        explicit VulkanDevice(VulkanInstance &inst);
        ~VulkanDevice() override;

        bool Init() override;
        std::string GetDeviceInfo() const override;
        void WaitIdle() const override;

        CommandPool *CreateCommandPool(QueueType type) override;

        VkDevice         GetNativeHandle() const { return device; }
        VkPhysicalDevice GetGpuHandle() const { return gpu; }

    private:
        bool CreateDevice();
        uint32_t GetQueueFamilyIndex(QueueType type) const;

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
