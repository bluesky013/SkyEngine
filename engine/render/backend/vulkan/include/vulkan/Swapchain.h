//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include <set>
#include <vector>
#include <rhi/Swapchain.h>
#include <vulkan/DevObject.h>
#include <vulkan/Image.h>
#include <vulkan/Semaphore.h>
#include <vulkan/vulkan.h>

#ifdef SKY_ENABLE_XR
#include <rhi/XRInterface.h>
#endif


namespace sky::vk {

    class Device;
    class Queue;

    class SwapChain : public rhi::SwapChain, public DevObject {
    public:
        ~SwapChain() override;

        struct VkDescriptor {
            void                         *window          = nullptr;
            uint32_t                      width           = 1;
            uint32_t                      height          = 1;
            VkFormat                      preferredFormat = VK_FORMAT_R8G8B8A8_UNORM;
            VkPresentModeKHR              preferredMode   = VK_PRESENT_MODE_IMMEDIATE_KHR;
            VkSurfaceTransformFlagBitsKHR preTransform    = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
            VkCompositeAlphaFlagBitsKHR   compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        };

        void Resize(uint32_t width, uint32_t height, void* window) override;

        // for vulkan
        VkSwapchainKHR GetNativeHandle() const { return swapChain; };
        VkResult AcquireNext(const SemaphorePtr& semaphore, uint32_t &next) const;

        // for rhi
        uint32_t AcquireNextImage(const rhi::SemaphorePtr &semaphore) override;
        void Present(rhi::Queue &queue, const rhi::PresentInfo &info) override;

        uint32_t GetImageCount() const override;
        rhi::PixelFormat GetFormat() const override;
        const rhi::Extent2D &GetExtent() const override;
        rhi::ImagePtr GetImage(uint32_t index) const override { return images[index]; }
        rhi::ImageViewPtr GetImageView(uint32_t index) const override { return imageViews[index]; }

        bool HasDepthStencilImage() const override { return false; }
        rhi::ImagePtr GetDepthStencilImage() const override { return {}; }
    private:
        friend class Device;
        explicit SwapChain(Device &);

        bool Init(const Descriptor &);

        bool CreateSurface();
        void DestroySurface();

        bool CreateSwapChain();
        void DestroySwapChain();

        VkSurfaceKHR             surface;
        VkSwapchainKHR           swapChain;
        Queue                   *queue;
        uint32_t                 imageCount;
        VkExtent2D               extent;
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR       format;
        VkPresentModeKHR         mode;
        std::vector<ImagePtr>    images;
        std::vector<rhi::ImageViewPtr> imageViews;
        VkDescriptor             descriptor;
    };

    using SwapChainPtr = std::shared_ptr<SwapChain>;

#ifdef SKY_ENABLE_XR
    class XRSwapChain : public rhi::XRSwapChain, public DevObject {
    public:
        explicit XRSwapChain(Device &dev);
        ~XRSwapChain() override;

        uint32_t AcquireNextImage() override;
        void Present() override;

        uint32_t GetArrayLayers() const override { return arrayLayers; }
        uint32_t GetImageCount() const override { return static_cast<uint32_t>(images.size()); }
        rhi::PixelFormat GetFormat() const override { return format; }
        rhi::ImagePtr GetImage(uint32_t index) const override { return images[index]; }
        rhi::ImageViewPtr GetImageView(uint32_t index) const override { return imageViews[index]; }
        const rhi::Extent2D &GetExtent() const override { return extent; }
    private:
        friend class Device;
        bool Init(const Descriptor &);

        rhi::PixelFormat format;
        rhi::Extent2D extent;
        uint32_t arrayLayers;
        std::vector<ImagePtr> images;
        std::vector<rhi::ImageViewPtr> imageViews;
    };
    using XRSwapChainPtr = std::shared_ptr<XRSwapChain>;
#endif
} // namespace sky::vk
