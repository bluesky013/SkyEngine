//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Swapchain.h"
#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "vulkan/Queue.h"
#include "vulkan/ImageView.h"
#include "core/logger/Logger.h"
#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#else
#endif

static const char* TAG = "Driver";

namespace sky::drv {

    Swapchain::Swapchain(Device& dev)
        : DevObject(dev)
        , surface(VK_NULL_HANDLE)
        , swapchain(VK_NULL_HANDLE)
        , queue(nullptr)
        , imageCount(0)
        , currentImage(0)
        , extent{1, 1}
    {
    }

    Swapchain::~Swapchain()
    {
        DestroySwapchain();
        DestroySurface();
    }

    bool Swapchain::Init(const Descriptor& des)
    {
        if (!CreateSurface(des)) {
            return false;
        }

        if (!CreateSwapchain(des)) {
            return false;
        }
        return true;
    }

    VkSwapchainKHR Swapchain::GetNativeHandle() const
    {
        return swapchain;
    }

    bool Swapchain::CreateSurface(const Descriptor& des)
    {
#ifdef _WIN32
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd      = (HWND)des.window;
        surfaceInfo.hinstance = GetModuleHandle(0);
        VkResult rst = vkCreateWin32SurfaceKHR(device.GetInstance(), &surfaceInfo, VKL_ALLOC, &surface);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create win32 surface failed, %d", rst);
            return false;
        }
#else
#endif
        return true;
    }

    void Swapchain::DestroySurface()
    {
        if (surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(device.GetInstance(), surface, VKL_ALLOC);
            surface = VK_NULL_HANDLE;
        }
    }

    bool Swapchain::CreateSwapchain(const Descriptor& des)
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
        swcInfo.imageUsage       = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        swcInfo.imageArrayLayers = 1;
        swcInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swcInfo.presentMode      = mode;
        swcInfo.clipped          = VK_TRUE;
        swcInfo.minImageCount    = imageCount;
        swcInfo.preTransform     = des.preTransform;
        swcInfo.compositeAlpha   = des.compositeAlpha;
        
        VkResult rst = vkCreateSwapchainKHR(device.GetNativeHandle(), &swcInfo, VKL_ALLOC, &swapchain);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create swapchain failed-%d", rst);
            return false;
        }

        std::vector<VkImage> images;
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &num, nullptr);
        images.resize(num);
        views.resize(num);
        vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &num, images.data());
        ImageView::Descriptor viewDes = {};
        viewDes.format = format.format;

        for (auto& image : images) {
            auto view = new ImageView(device, image);
            if (!view->Init(viewDes)) {
                return false;
            }
        }

        return true;
    }

    void Swapchain::DestroySwapchain()
    {
        if (swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device.GetNativeHandle(), swapchain, VKL_ALLOC);
            swapchain = VK_NULL_HANDLE;
        }
    }
}