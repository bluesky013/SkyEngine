//
// Aurora Vulkan SwapChain.
//

#pragma once

#include <VulkanFunctions.h>
#include <VulkanImage.h>
#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>
#include <aurora/rhi/SwapChain.h>
#include <memory>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanSwapChain : public SwapChain {
    public:
        explicit VulkanSwapChain(VulkanDevice &dev);
        ~VulkanSwapChain() override;

        bool Init(const Descriptor &desc);
        Device                *GetDevice() const noexcept override;
        SurfaceQueryResult     QuerySurface() const noexcept override;
        SwapChainAcquireResult TryAcquireFrame(uint64_t timeoutNs) noexcept override;
        PresentationResult     TryPresentFrame(uint64_t serial, bool gpuComplete) noexcept override;
        FenceQueryResult       RetireFrame(uint64_t serial, bool abandon) noexcept override;
        PresentationResult     TryResize(uint32_t width, uint32_t height) noexcept override;
        PresentationResult     TryClose() noexcept override;

        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void        Resize(uint32_t width, uint32_t height) override;
        Image*      GetImage(uint32_t index) const override;
        uint32_t    GetImageCount() const override { return static_cast<uint32_t>(images.size()); }
        PixelFormat GetFormat() const override { return pixelFormat; }
        Extent2D    GetExtent() const override { return {extent.width, extent.height}; }
        SwapChainStatus GetStatus() const override;
        Extent2D    GetSurfaceSize() const override;

    private:
        struct PresentationFrame {
            uint64_t     serial     = 0;
            uint32_t     imageIndex = INVALID_INDEX;
            SemaphorePtr acquired, rendered;
            FencePtr     acquireFence, presentFence;
            bool         presented = false;
            GpuError     error;
        };
        std::vector<PresentationFrame> presentationFrames;
        uint64_t                       presentationSerial = 0;
        bool                           checkedProtocol    = false;
        bool                           directProtocol     = false;
        VkResult                       creationResult     = VK_SUCCESS;
        bool                           CreateSurface(void *window);
        VkResult                       QueryCapabilities(uint32_t width, uint32_t height, VkSwapchainCreateInfoKHR &) const;
        VkResult                       QueryPresentationOptions(VkSwapchainCreateInfoKHR &) const;
        VkResult                       QueryConfiguration(uint32_t width, uint32_t height, VkSwapchainCreateInfoKHR &) const;
        bool                           CreateSwapchain(uint32_t width, uint32_t height);
        void                           DestroySwapchain();
        void                           DestroySurface();

        VulkanDevice    &device;
        VkSurfaceKHR     surface     = VK_NULL_HANDLE;
        VkSwapchainKHR   swapchain   = VK_NULL_HANDLE;
        VkSurfaceFormatKHR  surfaceFormat = {};
        VkPresentModeKHR    presentMode   = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D          extent        = {1, 1};
        PixelFormat         pixelFormat   = PixelFormat::BGRA8_UNORM;
        SwapChainStatus     status        = SwapChainStatus::OK;
        void                    *window          = nullptr;
        PresentMode              preferredMode   = PresentMode::IMMEDIATE;
        PixelFormat              preferredFormat = PixelFormat::BGRA8_UNORM;
        VkSwapchainCreateInfoKHR applied{};
        uint64_t                 surfaceRevision = 0;
        mutable bool             presentationOptionsDirty = false;

        std::vector<std::unique_ptr<VulkanImage>> images;
    };

} // namespace sky::aurora
