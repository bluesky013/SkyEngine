//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <vulkan/ImageView.h>
#include <vulkan/Semaphore.h>
#include <vector>
#include <set>

namespace sky::drv {

    class Device;
    class Queue;
    class SwapChain;

    class SwapChainListener {
    public:
        SwapChainListener() = default;
        virtual ~SwapChainListener() = default;
        virtual void OnResize(SwapChain&) {}
    };

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
            std::vector<Semaphore*> signals;
        };

        bool Init(const Descriptor&);

        VkSwapchainKHR GetNativeHandle() const;

        VkFormat GetFormat() const;

        const VkExtent2D& GetExtent() const;

        const std::vector<ImageViewPtr>& GetViews() const;

        void Present(const PresentInfo&) const;

        VkResult AcquireNext() const;

        const Semaphore* GetAvailableSemaphore() const;

        const ImageView* GetCurrentImageView() const;

        void Resize(uint32_t width, uint32_t height);

        void RegisterListener(SwapChainListener* listener);

        void UnRegisterListener(SwapChainListener* listener);

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
        mutable uint32_t currentImage;
        VkExtent2D extent;
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR format;
        VkPresentModeKHR mode;
        std::vector<ImageViewPtr> views;
        SemaphorePtr imageAvailable;
        Descriptor descriptor;
        std::set<SwapChainListener*> listeners;
    };

    using SwapChainPtr = std::shared_ptr<SwapChain>;

}
