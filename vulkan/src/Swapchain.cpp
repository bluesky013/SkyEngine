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
        , currentImage(0)
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
        if (!CreateSurface(des)) {
            return false;
        }

        if (!CreateSwapChain(des)) {
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

    VkExtent2D SwapChain::GetExtent() const
    {
        return extent;
    }

    const std::vector<ImageView*>& SwapChain::GetViews() const
    {
        return views;
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
            &currentImage
        };
        vkQueuePresentKHR(queue->GetNativeHandle(), &presentInfo);
    }

    bool SwapChain::CreateSwapChain(const Descriptor& des)
    {
        std::vector<VkQueueFlags> preferred = {
            VK_QUEUE_TRANSFER_BIT,
            VK_QUEUE_COMPUTE_BIT,
            VK_QUEUE_GRAPHICS_BIT
        };
        VkPhysicalDevice gpu = device.GetGpuHandle();
        auto surfaceCheck = [this, gpu](uint32_t index) {
            VkBool32 support = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(gpu, index, surface, &support);
            return support;
        };

        for (auto& pref : preferred) {
            queue = device.GetQueue({pref});
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
            if (f.format == des.preferredFormat) {
                format = f;
                break;
            }
        }

        mode = presentModes[0];
        for (auto& m : presentModes) {
            if (m == des.preferredMode) {
                mode = m;
                break;
            }
        }
        imageCount = std::min(capabilities.minImageCount + 1, capabilities.maxImageCount);
        extent = capabilities.currentExtent;

        VkSwapchainCreateInfoKHR swcInfo = {};
        swcInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swcInfo.surface          = surface;
        swcInfo.imageFormat      = format.format;
        swcInfo.imageColorSpace  = format.colorSpace;
        swcInfo.imageExtent      = extent;
        swcInfo.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swcInfo.imageArrayLayers = 1;
        swcInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swcInfo.presentMode      = mode;
        swcInfo.clipped          = VK_TRUE;
        swcInfo.minImageCount    = imageCount;
        swcInfo.preTransform     = des.preTransform;
        swcInfo.compositeAlpha   = des.compositeAlpha;
        
        VkResult rst = vkCreateSwapchainKHR(device.GetNativeHandle(), &swcInfo, VKL_ALLOC, &swapChain);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create swapChain failed-%d", rst);
            return false;
        }

        LOG_I(TAG, "create swapChain format-%d width-%u, height-%u, imageCount-%u", format.format, extent.width, extent.height, imageCount);

        std::vector<VkImage> images;
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &num, nullptr);
        images.resize(num);
        views.reserve(num);
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapChain, &num, images.data());
        ImageView::Descriptor viewDes = {};
        viewDes.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewDes.format = format.format;

        for (auto& img : images) {
            viewDes.image = img;
            views.emplace_back(device.CreateDeviceObject<ImageView>(viewDes));
        }

        imageAvailable = device.CreateDeviceObject<Semaphore>(Semaphore::Descriptor{});

        return true;
    }

    void SwapChain::DestroySwapChain()
    {
        for (auto& view : views) {
            delete view;
        }
        views.clear();

        if (swapChain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device.GetNativeHandle(), swapChain, VKL_ALLOC);
            swapChain = VK_NULL_HANDLE;
        }
    }

    VkResult SwapChain::AcquireNext() const
    {
        return vkAcquireNextImageKHR(device.GetNativeHandle(), swapChain, UINT64_MAX,
            imageAvailable->GetNativeHandle(),
            VK_NULL_HANDLE, &currentImage);
    }

    Semaphore* SwapChain::GetAvailableSemaphore() const
    {
        return imageAvailable;
    }

    ImageView* SwapChain::GetCurrentImageView() const
    {
        AcquireNext();
        return views[currentImage];
    }
}