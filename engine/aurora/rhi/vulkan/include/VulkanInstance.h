//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Instance.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanInstance : public Instance::Impl {
    public:
        VulkanInstance() = default;
        ~VulkanInstance() override;

        bool Init(const Instance::Descriptor &desc) override;
        Device *CreateDevice() override;

        VkInstance       GetNativeHandle() const { return instance; }
        VkPhysicalDevice GetActiveGpu() const { return activeGpu; }
        bool             IsDebugEnabled() const { return enableDebugLayer; }

    private:
        bool CreateInstance(const Instance::Descriptor &desc);
        bool SetupDebugMessenger();
        bool PickPhysicalDevice();

        VkInstance               instance       = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice         activeGpu      = VK_NULL_HANDLE;

        std::vector<VkPhysicalDevice> physicalDevices;
        std::vector<const char*>      enabledLayers;
        std::vector<const char*>      enabledExtensions;

        bool enableDebugLayer = false;
    };

} // namespace sky::aurora