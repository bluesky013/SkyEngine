//
// Aurora Vulkan SwapChain.
//

#include <VulkanSwapChain.h>
#include <VulkanDevice.h>
#include <VulkanInstance.h>
#include <VulkanQueue.h>
#include <VulkanFence.h>
#include <VulkanSemaphore.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>

#include <algorithm>

#if defined(VK_USE_PLATFORM_METAL_EXT)
    #include <vulkan/vulkan_metal.h>
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    #include <vulkan/vulkan_win32.h>
#endif
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
    #include <vulkan/vulkan_android.h>
#endif

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    static PixelFormat ToPixelFormat(VkFormat fmt)
    {
        switch (fmt) {
        case VK_FORMAT_R8G8B8A8_UNORM:    return PixelFormat::RGBA8_UNORM;
        case VK_FORMAT_R8G8B8A8_SRGB:     return PixelFormat::RGBA8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM:    return PixelFormat::BGRA8_UNORM;
        case VK_FORMAT_B8G8R8A8_SRGB:     return PixelFormat::BGRA8_SRGB;
        default:                          return PixelFormat::UNDEFINED;
        }
    }

    VulkanSwapChain::VulkanSwapChain(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanSwapChain::~VulkanSwapChain()
    {
        DestroySwapchain();
        DestroySurface();
    }

    bool VulkanSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a non-null window pointer");
            return false;
        }
        if (!CreateSurface(desc.window)) {
            return false;
        }
        return CreateSwapchain(desc.width, desc.height, desc.preferredMode, desc.preferredFormat);
    }

    bool VulkanSwapChain::CreateSurface(void *window)
    {
        VulkanInstance &inst = device.GetVulkanInstance();
        const auto &instFn   = inst.GetInstanceFn();
        VkInstance vkInstance = inst.GetNativeHandle();

#if defined(VK_USE_PLATFORM_METAL_EXT)
        VkMetalSurfaceCreateInfoEXT info = {};
        info.sType  = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
        info.pLayer = static_cast<const CAMetalLayer *>(window);
        if (instFn.vkCreateMetalSurfaceEXT == nullptr) {
            LOG_E(TAG, "vkCreateMetalSurfaceEXT not loaded");
            return false;
        }
        const VkResult r = instFn.vkCreateMetalSurfaceEXT(vkInstance, &info, nullptr, &surface);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateMetalSurfaceEXT failed: %d", r);
            return false;
        }
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
        VkWin32SurfaceCreateInfoKHR info = {};
        info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        info.hinstance = GetModuleHandleW(nullptr);
        info.hwnd      = static_cast<HWND>(window);
        const VkResult r = instFn.vkCreateWin32SurfaceKHR(vkInstance, &info, nullptr, &surface);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateWin32SurfaceKHR failed: %d", r);
            return false;
        }
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        VkAndroidSurfaceCreateInfoKHR info = {};
        info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        info.window = static_cast<ANativeWindow *>(window);
        const VkResult r = instFn.vkCreateAndroidSurfaceKHR(vkInstance, &info, nullptr, &surface);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateAndroidSurfaceKHR failed: %d", r);
            return false;
        }
#else
        (void)window;
        (void)instFn;
        (void)vkInstance;
        LOG_E(TAG, "no platform surface support compiled in");
        return false;
#endif

        // Sanity-check that the graphics queue family supports presentation.
        VkBool32 supported = VK_FALSE;
        instFn.vkGetPhysicalDeviceSurfaceSupportKHR(device.GetGpuHandle(),
            device.GetQueueFamilyIndex(QueueType::GRAPHICS), surface, &supported);
        if (!supported) {
            LOG_E(TAG, "graphics queue family does not support presentation on this surface");
            return false;
        }
        return true;
    }

    bool VulkanSwapChain::CreateSwapchain(uint32_t width, uint32_t height, PresentMode preferredMode, PixelFormat preferredFormat)
    {
        VulkanInstance &inst = device.GetVulkanInstance();
        const auto &instFn   = inst.GetInstanceFn();
        const auto &devFn    = device.GetDeviceFn();
        VkPhysicalDevice gpu = device.GetGpuHandle();

        // capabilities
        VkSurfaceCapabilitiesKHR caps = {};
        instFn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);

        // pick format
        uint32_t formatCount = 0;
        instFn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        instFn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, formats.data());

        const VkFormat preferredVkFormat = FromPixelFormat(preferredFormat);
        surfaceFormat = formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR}
                                        : formats.front();
        for (const auto &f : formats) {
            if (f.format == preferredVkFormat && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = f;
                break;
            }
        }
        pixelFormat = ToPixelFormat(surfaceFormat.format);

        // pick present mode
        uint32_t pmCount = 0;
        instFn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(pmCount);
        instFn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &pmCount, presentModes.data());

        const VkPresentModeKHR desired = (preferredMode == PresentMode::VSYNC) ? VK_PRESENT_MODE_FIFO_KHR
                                                                               : VK_PRESENT_MODE_IMMEDIATE_KHR;
        presentMode = VK_PRESENT_MODE_FIFO_KHR;     // FIFO is always supported
        for (auto m : presentModes) {
            if (m == desired) { presentMode = m; break; }
        }

        // pick extent
        if (caps.currentExtent.width != UINT32_MAX) {
            extent = caps.currentExtent;
        } else {
            extent.width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
            extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }

        const uint32_t minImages = caps.minImageCount + 1;
        const uint32_t numImages = (caps.maxImageCount > 0)
                                       ? std::min(minImages, caps.maxImageCount)
                                       : minImages;

        VkSwapchainCreateInfoKHR ci = {};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = surface;
        ci.minImageCount    = numImages;
        ci.imageFormat      = surfaceFormat.format;
        ci.imageColorSpace  = surfaceFormat.colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.preTransform     = caps.currentTransform;
        ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode      = presentMode;
        ci.clipped          = VK_TRUE;
        ci.oldSwapchain     = VK_NULL_HANDLE;

        const VkResult r = devFn.vkCreateSwapchainKHR(device.GetNativeHandle(), &ci, nullptr, &swapchain);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateSwapchainKHR failed: %d", r);
            return false;
        }

        uint32_t imageCount = 0;
        devFn.vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapImages(imageCount);
        devFn.vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &imageCount, swapImages.data());

        images.clear();
        images.reserve(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            auto img = std::make_unique<VulkanImage>(device);
            img->InitFromSwapChain(swapImages[i], surfaceFormat.format, pixelFormat,
                                   {extent.width, extent.height, 1});
            images.push_back(std::move(img));
        }

        LOG_I(TAG, "swapchain created: %ux%u format=%d images=%u presentMode=%d",
              extent.width, extent.height, surfaceFormat.format, imageCount, presentMode);
        return true;
    }

    void VulkanSwapChain::DestroySwapchain()
    {
        images.clear();   // release borrowed wrappers (do not vkDestroyImage)
        if (swapchain != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroySwapchainKHR(device.GetNativeHandle(), swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanSwapChain::DestroySurface()
    {
        if (surface != VK_NULL_HANDLE) {
            device.GetVulkanInstance().GetInstanceFn().vkDestroySurfaceKHR(
                device.GetVulkanInstance().GetNativeHandle(), surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    }

    uint32_t VulkanSwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs)
    {
        VkSemaphore vkSema = VK_NULL_HANDLE;
        if (signalSema != nullptr) {
            vkSema = static_cast<VulkanSemaphore *>(signalSema)->GetNativeHandle();
        }
        VkFence vkFence = VK_NULL_HANDLE;
        if (fence != nullptr) {
            vkFence = static_cast<VulkanFence *>(fence)->GetNativeHandle();
        }

        uint32_t index = 0;
        const VkResult r = device.GetDeviceFn().vkAcquireNextImageKHR(device.GetNativeHandle(),
            swapchain, timeoutNs, vkSema, vkFence, &index);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_TIMEOUT) {
            return INVALID_INDEX;
        }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            LOG_E(TAG, "vkAcquireNextImageKHR failed: %d", r);
            return INVALID_INDEX;
        }
        return index;
    }

    void VulkanSwapChain::Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        std::vector<VkSemaphore> waits;
        waits.reserve(numWaitSemas);
        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            if (waitSemas[i] != nullptr) {
                waits.push_back(static_cast<VulkanSemaphore *>(waitSemas[i])->GetNativeHandle());
            }
        }

        VkPresentInfoKHR info = {};
        info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = static_cast<uint32_t>(waits.size());
        info.pWaitSemaphores    = waits.data();
        info.swapchainCount     = 1;
        info.pSwapchains        = &swapchain;
        info.pImageIndices      = &imageIndex;

        VkQueue vkQueue = static_cast<VulkanQueue *>(
            device.GetQueue(QueueType::GRAPHICS))->GetNativeHandle();
        const VkResult r = device.GetDeviceFn().vkQueuePresentKHR(vkQueue, &info);
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR && r != VK_ERROR_OUT_OF_DATE_KHR) {
            LOG_E(TAG, "vkQueuePresentKHR failed: %d", r);
        }
    }

    void VulkanSwapChain::Resize(uint32_t width, uint32_t height)
    {
        // VulkanDevice::WaitIdle is private; use the device-fn directly.
        device.GetDeviceFn().vkDeviceWaitIdle(device.GetNativeHandle());
        DestroySwapchain();
        // preserve the surface; recreate swapchain only
        CreateSwapchain(width, height,
                        (presentMode == VK_PRESENT_MODE_FIFO_KHR) ? PresentMode::VSYNC : PresentMode::IMMEDIATE,
                        pixelFormat);
    }

    Image *VulkanSwapChain::GetImage(uint32_t index) const
    {
        if (index >= images.size()) {
            return nullptr;
        }
        return images[index].get();
    }

} // namespace sky::aurora
