//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <vulkan/Image.h>
#include <vulkan/Semaphore.h>
#include <vector>
#include <set>

namespace sky::drv {

    class Device;
    class Queue;
    class SwapChain;

    class SwapChain : public DevObject {
    public:
        ~SwapChain();

        struct Descriptor {
            void* window = nullptr;
            uint32_t width = 1;
            uint32_t height = 1;
            VkFormat preferredFormat = VK_FORMAT_R8G8B8A8_UNORM;
            VkPresentModeKHR preferredMode = VK_PRESENT_MODE_MAILBOX_KHR;
            VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        };

        struct PresentInfo {
            std::vector<SemaphorePtr> signals;
            uint32_t imageIndex = 0;
        };

        bool Init(const Descriptor&);

        VkSwapchainKHR GetNativeHandle() const;

        VkFormat GetFormat() const;

        const VkExtent2D& GetExtent() const;

        void Present(const PresentInfo&) const;

        VkResult AcquireNext(SemaphorePtr semaphore, uint32_t& next) const;

        void Resize(uint32_t width, uint32_t height);

        ImagePtr GetImage(uint32_t image) const;

        uint32_t GetImageCount() const;

    private:
        friend class Device;
        SwapChain(Device&);

        bool CreateSurface();
        void DestroySurface();

        bool CreateSwapChain();
        void DestroySwapChain();

        VkSurfaceKHR surface;
        VkSwapchainKHR swapChain;
        Queue* queue;
        uint32_t imageCount;
        VkExtent2D extent;
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR format;
        VkPresentModeKHR mode;
        std::vector<ImagePtr> images;
        Descriptor descriptor;
    };

    using SwapChainPtr = std::shared_ptr<SwapChain>;

}
