//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Swapchain.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "vulkan/Queue.h"
#include "vulkan/ImageView.h"
#include "vulkan/Semaphore.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    SwapChain::SwapChain(Device& dev)
        : DevObject(dev)
        , surface(VK_NULL_HANDLE)
        , swapChain(VK_NULL_HANDLE)
        , queue(nullptr)
        , imageCount(0)
        , extent{1, 1}
    {
    }

    SwapChain::~SwapChain()
    {
        DestroySwapChain();
        DestroySurface();
    }

    bool SwapChain::Init(const Descriptor& des)
    {
        descriptor = des;
        if (!CreateSurface()) {
            return false;
        }

        if (!CreateSwapChain()) {
            return false;
        }
        return true;
    }

    VkSwapchainKHR SwapChain::GetNativeHandle() const
    {
        return swapChain;
    }

    void SwapChain::DestroySurface()
    {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(device.GetInstance(), surface, VKL_ALLOC);
            surface = VK_NULL_HANDLE;
        }
    }

    VkFormat SwapChain::GetFormat() const
    {
        return format.format;
    }

    const VkExtent2D& SwapChain::GetExtent() const
    {
        return extent;
    }

    void SwapChain::Present(const PresentInfo& info) const
    {
        std::vector<VkSemaphore> semaphores;
        for (auto& sem : info.signals) {
            semaphores.emplace_back(sem->GetNativeHandle());
        }

        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            (uint32_t)semaphores.size(),
            semaphores.data(),
            1,
            &swapChain,
            &info.imageIndex
        };
        vkQueuePresentKHR(queue->GetNativeHandle(), &presentInfo);
    }

    bool SwapChain::CreateSwapChain()
    {
        std::vector<VkQueueFlags> preferred = {
            VK_QUEUE_GRAPHICS_BIT,
            VK_QUEUE_TRANSFER_BIT,
            VK_QUEUE_COMPUTE_BIT,
        };
        VkPhysicalDevice gpu = device.GetGpuHandle();
        auto surfaceCheck = [this, gpu](uint32_t index) {
            VkBool32 support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, index, surface, &support);
            return support;
        };

        for (auto& pref : preferred) {
            queue = device.GetQueue(pref);
            if (queue != nullptr && surfaceCheck(queue->GetQueueFamilyIndex())) {
                break;
            }
        }
        if (queue == nullptr) {
            return false;
        }

        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;

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
        for (auto& f : formats) {
            if (f.format == descriptor.preferredFormat) {
                format = f;
                break;
            }
        }

        mode = presentModes[0];
        for (auto& m : presentModes) {
            if (m == descriptor.preferredMode) {
                mode = m;
                break;
            }
        }
        imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);
        extent.width = std::max(descriptor.width, capabilities.currentExtent.width);
        extent.height = std::max(descriptor.height, capabilities.currentExtent.height);

        extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
        extent.height = std::min(extent.height, capabilities.maxImageExtent.height);

        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = VkExtent3D{extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkSwapchainCreateInfoKHR swcInfo = {};
        swcInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swcInfo.surface          = surface;
        swcInfo.imageFormat      = imageInfo.format = format.format;
        swcInfo.imageColorSpace  = format.colorSpace;
        swcInfo.imageExtent      = extent;
        swcInfo.imageUsage       = imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swcInfo.imageArrayLayers = 1;
        swcInfo.imageSharingMode = imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swcInfo.presentMode      = mode;
        swcInfo.clipped          = VK_TRUE;
        swcInfo.minImageCount    = imageCount;
        swcInfo.preTransform     = descriptor.preTransform;
        swcInfo.compositeAlpha   = descriptor.compositeAlpha;
        
        VkResult rst = vkCreateSwapchainKHR(device.GetNativeHandle(), &swcInfo, VKL_ALLOC, &swapChain);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create swapChain failed-%d", rst);
            return false;
        }

        LOG_I(TAG, "create swapChain format-%d width-%u, height-%u, imageCount-%u", format.format, extent.width, extent.height, imageCount);

        std::vector<VkImage> vImages;
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &num, nullptr);
        vImages.resize(num);
        images.resize(num);
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &num, vImages.data());
        for (uint32_t i = 0; i < num; ++i) {
            images[i] = std::shared_ptr<Image>(new Image(device, vImages[i]));
            images[i]->imageInfo = imageInfo;
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

    VkResult SwapChain::AcquireNext(SemaphorePtr semaphore,  uint32_t& next) const
    {
        return vkAcquireNextImageKHR(device.GetNativeHandle(), swapChain, UINT64_MAX,
            semaphore->GetNativeHandle(),
            VK_NULL_HANDLE, &next);
    }

    void SwapChain::Resize(uint32_t width, uint32_t height)
    {
        descriptor.width = width;
        descriptor.height = height;
        if (queue != nullptr) {
            vkQueueWaitIdle(queue->GetNativeHandle());
        }
        DestroySwapChain();
        CreateSwapChain();
    }

    ImagePtr SwapChain::GetImage(uint32_t image) const
    {
        return images[image];
    }

    uint32_t SwapChain::GetImageCount() const
    {
        return imageCount;
    }
}