//
// Created by Zach Lee on 2021/11/7.
//

#include <vulkan/Swapchain.h>
#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>
#include <vulkan/ImageView.h>
#include <vulkan/Semaphore.h>
#include <vulkan/Conversion.h>
#include <vulkan/Instance.h>

#ifdef SKY_ENABLE_XR
#include <openxr/openxr_platform.h>
#endif

static const char *TAG = "Vulkan";

namespace sky::vk {

    SwapChain::SwapChain(Device &dev)
    : DevObject(dev), surface(VK_NULL_HANDLE), swapChain(VK_NULL_HANDLE), queue(nullptr), imageCount(0), extent{1, 1}
    {
    }

    SwapChain::~SwapChain()
    {
        DestroySwapChain();
        DestroySurface();
    }

    bool SwapChain::Init(const Descriptor &des)
    {
        descriptor.window          = des.window;
        descriptor.width           = des.width;
        descriptor.height          = des.height;
        descriptor.preferredFormat = FromRHI(des.preferredFormat);
        descriptor.preferredMode   = des.preferredMode == rhi::PresentMode::IMMEDIATE ? VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;
        descriptor.preTransform    = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        descriptor.compositeAlpha  = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        if (!CreateSurface()) {
            return false;
        }

        if (!CreateSwapChain()) {
            return false;
        }
        return true;
    }

    void SwapChain::DestroySurface()
    {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(device.GetInstanceId(), surface, VKL_ALLOC);
            surface = VK_NULL_HANDLE;
        }
    }

    bool SwapChain::CreateSwapChain()
    {
        std::pair<VkQueueFlags, VkQueueFlags> preferred[] = {
            {VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, 0}, {VK_QUEUE_GRAPHICS_BIT, 0}, {VK_QUEUE_TRANSFER_BIT, 0}};
        VkPhysicalDevice gpu          = device.GetGpuHandle();
        auto             surfaceCheck = [this, gpu](uint32_t index) {
            VkBool32 support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, index, surface, &support);
            return support;
        };

        for (auto &pref : preferred) {
            queue = device.GetQueue(pref.first, pref.second);
            if (queue != nullptr && (surfaceCheck(queue->GetQueueFamilyIndex()) != 0u)) {
                break;
            }
        }
        if (queue == nullptr) {
            return false;
        }

        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &capabilities);

        uint32_t num = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &num, nullptr);
        formats.resize(num);
        vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &num, formats.data());

        num = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &num, nullptr);
        presentModes.resize(num);
        vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &num, presentModes.data());

        if (formats.empty() || presentModes.empty()) {
            return false;
        }
        format = formats[0];
        for (auto &f : formats) {
            if (f.format == descriptor.preferredFormat) {
                format = f;
                break;
            }
        }

        mode = presentModes[0];
        for (auto &m : presentModes) {
            if (m == descriptor.preferredMode) {
                mode = m;
                break;
            }
        }
        imageCount    = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);
        extent.width  = std::max(descriptor.width, capabilities.currentExtent.width);
        extent.height = std::max(descriptor.height, capabilities.currentExtent.height);

        extent.width  = std::min(extent.width, capabilities.maxImageExtent.width);
        extent.height = std::min(extent.height, capabilities.maxImageExtent.height);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageInfo.extent            = VkExtent3D{extent.width, extent.height, 1};
        imageInfo.mipLevels         = 1;
        imageInfo.arrayLayers       = 1;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkSwapchainCreateInfoKHR swcInfo = {};
        swcInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swcInfo.surface                  = surface;
        swcInfo.imageFormat = imageInfo.format = format.format;
        swcInfo.imageColorSpace                = format.colorSpace;
        swcInfo.imageExtent                    = extent;
        swcInfo.imageUsage                     = imageInfo.usage;
        swcInfo.imageArrayLayers               = 1;
        swcInfo.imageSharingMode = imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swcInfo.presentMode                              = mode;
        swcInfo.clipped                                  = VK_TRUE;
        swcInfo.minImageCount                            = imageCount;
        swcInfo.preTransform                             = descriptor.preTransform;
        swcInfo.compositeAlpha                           = descriptor.compositeAlpha;

        auto rst = vkCreateSwapchainKHR(device.GetNativeHandle(), &swcInfo, VKL_ALLOC, &swapChain);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create swapChain failed-%d", rst);
            return false;
        }

        LOG_I(TAG, "create swapChain format-%d width-%u, height-%u, imageCount-%u", format.format, extent.width, extent.height, imageCount);

        std::vector<VkImage> vImages;
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &imageCount, nullptr);
        vImages.resize(imageCount);
        images.resize(imageCount);
        imageViews.resize(imageCount);
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &imageCount, vImages.data());
        for (uint32_t i = 0; i < imageCount; ++i) {
            images[i]            = std::shared_ptr<Image>(new Image(device, vImages[i]));
            images[i]->imageInfo = imageInfo;
            imageViews[i] = images[i]->CreateView({});
        }

        return true;
    }

    void SwapChain::DestroySwapChain()
    {
        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device.GetNativeHandle(), swapChain, VKL_ALLOC);
            swapChain = VK_NULL_HANDLE;
        }
    }

    VkResult SwapChain::AcquireNext(const SemaphorePtr& semaphore, uint32_t &next) const
    {
        return vkAcquireNextImageKHR(device.GetNativeHandle(), swapChain, UINT64_MAX, semaphore->GetNativeHandle(), VK_NULL_HANDLE, &next);
    }

    void SwapChain::Resize(uint32_t width, uint32_t height, void* window)
    {
        descriptor.width  = width;
        descriptor.height = height;
        descriptor.window = window;
        if (queue != nullptr) {
            vkQueueWaitIdle(queue->GetNativeHandle());
        }
        DestroySwapChain();
        CreateSwapChain();
    }

    uint32_t SwapChain::GetImageCount() const
    {
        return imageCount;
    }

    rhi::PixelFormat SwapChain::GetFormat() const
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM) {
            return rhi::PixelFormat::BGRA8_UNORM;
        }
        return rhi::PixelFormat::RGBA8_UNORM;
    }

    const rhi::Extent2D &SwapChain::GetExtent() const
    {
        return *static_cast<const rhi::Extent2D*>(reinterpret_cast<const void*>(&extent));
    }

    uint32_t SwapChain::AcquireNextImage(const rhi::SemaphorePtr &semaphore)
    {
        uint32_t next = 0;
        AcquireNext(std::static_pointer_cast<Semaphore>(semaphore), next);
        return next;
    }

    void SwapChain::Present(rhi::Queue &presentQueue, const rhi::PresentInfo &info)
    {
        std::vector<VkSemaphore> semaphores;
        for (const auto &sem : info.semaphores) {
            semaphores.emplace_back(std::static_pointer_cast<Semaphore>(sem)->GetNativeHandle());
        }

        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(semaphores.size());
        presentInfo.pWaitSemaphores = semaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &info.imageIndex;
        vkQueuePresentKHR(static_cast<Queue&>(presentQueue).GetNativeHandle(), &presentInfo);
    }

#ifdef SKY_ENABLE_XR
    XRSwapChain::XRSwapChain(Device &dev) : DevObject(dev)
    {
    }

    XRSwapChain::~XRSwapChain()
    {
        auto *xrInterface = device.GetInstance().GetXRInterface();
        xrInterface->DestroyXrSwapChain(swapChain);
    }

    uint32_t XRSwapChain::AcquireNextImage()
    {
        return swapChain->AcquireNextImage();
    }

    void XRSwapChain::Present()
    {
        swapChain->Present();
    }

    bool XRSwapChain::Init(const Descriptor &desc)
    {
        auto *xrInterface = device.GetInstance().GetXRInterface();
        swapChain = xrInterface->CreateXrSwapChain({static_cast<int64_t>(FromRHI(desc.format))});
        XrSwapchain xrSwapChain = swapChain->GetHandle(0);
        format = desc.format;

        auto fn = reinterpret_cast<PFN_xrEnumerateSwapchainImages>(xrInterface->GetFunction("xrEnumerateSwapchainImages"));
        uint32_t imageCount = 0;
        fn(xrSwapChain, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageVulkanKHR> xrImages(imageCount);
        std::vector<XrSwapchainImageBaseHeader*> headers(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            xrImages[i].type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
            headers[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&xrImages[i]);
        }
        fn(xrSwapChain, imageCount, &imageCount, headers[0]);

        extent = {swapChain->GetWidth(), swapChain->GetHeight()};
        arrayLayers = swapChain->GetArrayLayers();

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageInfo.extent            = VkExtent3D{swapChain->GetWidth(), swapChain->GetHeight(), 1};
        imageInfo.mipLevels         = 1;
        imageInfo.arrayLayers       = arrayLayers;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.format            = static_cast<VkFormat>(swapChain->GetFormat());
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        rhi::ImageViewDesc viewInfo = {};
        viewInfo.subRange.layers = arrayLayers;
        viewInfo.viewType = rhi::ImageViewType::VIEW_2D_ARRAY;

        images.resize(imageCount);
        imageViews.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i) {
            images[i]            = std::shared_ptr<Image>(new Image(device, xrImages[i].image));
            images[i]->imageInfo = imageInfo;
            imageViews[i] = images[i]->CreateView(viewInfo);
        }

        return swapChain != nullptr;
    }
#endif

} // namespace sky::vk
