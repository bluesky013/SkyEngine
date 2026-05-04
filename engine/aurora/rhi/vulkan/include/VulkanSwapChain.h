//
// Aurora Vulkan SwapChain.
//

#pragma once

#include <aurora/rhi/SwapChain.h>
#include <VulkanFunctions.h>
#include <VulkanImage.h>
#include <memory>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanSwapChain : public SwapChain {
    public:
        explicit VulkanSwapChain(VulkanDevice &dev);
        ~VulkanSwapChain() override;

        bool Init(const Descriptor &desc);

        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void        Resize(uint32_t width, uint32_t height) override;
        Image*      GetImage(uint32_t index) const override;
        uint32_t    GetImageCount() const override { return static_cast<uint32_t>(images.size()); }
        PixelFormat GetFormat() const override { return pixelFormat; }
        Extent2D    GetExtent() const override { return {extent.width, extent.height}; }

    private:
        bool CreateSurface(void *window);
        bool CreateSwapchain(uint32_t width, uint32_t height, PresentMode preferredMode, PixelFormat preferredFormat);
        void DestroySwapchain();
        void DestroySurface();

        VulkanDevice    &device;
        VkSurfaceKHR     surface     = VK_NULL_HANDLE;
        VkSwapchainKHR   swapchain   = VK_NULL_HANDLE;
        VkSurfaceFormatKHR  surfaceFormat = {};
        VkPresentModeKHR    presentMode   = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D          extent        = {1, 1};
        PixelFormat         pixelFormat   = PixelFormat::BGRA8_UNORM;

        std::vector<std::unique_ptr<VulkanImage>> images;
    };

} // namespace sky::aurora
