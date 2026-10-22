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
    #include <android/native_window.h>
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
        // Terminal teardown alone may block; queue idle does not replace the presentation fence.
        if (std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }) &&
            device.GetDeviceFn().vkDeviceWaitIdle(device.GetNativeHandle()) == VK_SUCCESS)
            for (auto &frame : presentationFrames)
                if (frame.serial)
                    (frame.presented ? frame.presentFence : frame.acquireFence)->Wait();
        DestroySwapchain();
        DestroySurface();
    }

    bool VulkanSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a non-null window pointer");
            return false;
        }
        window          = desc.window;
        preferredMode   = desc.preferredMode;
        preferredFormat = desc.preferredFormat;
        if (!CreateSurface(desc.window)) {
            return false;
        }
        return CreateSwapchain(desc.width, desc.height);
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

    VkResult VulkanSwapChain::QueryCapabilities(uint32_t width, uint32_t height, VkSwapchainCreateInfoKHR &ci) const
    {
        if (!surface)
            return VK_ERROR_SURFACE_LOST_KHR;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
        if (!IsWindow(static_cast<HWND>(window)))
            return VK_ERROR_SURFACE_LOST_KHR;
        if (IsIconic(static_cast<HWND>(window)))
            return VK_NOT_READY;
        RECT rect{};
        if (!GetClientRect(static_cast<HWND>(window), &rect))
            return VK_ERROR_SURFACE_LOST_KHR;
        width  = static_cast<uint32_t>(std::max<LONG>(0, rect.right - rect.left));
        height = static_cast<uint32_t>(std::max<LONG>(0, rect.bottom - rect.top));
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
        width  = static_cast<uint32_t>(std::max(0, ANativeWindow_getWidth(static_cast<ANativeWindow *>(window))));
        height = static_cast<uint32_t>(std::max(0, ANativeWindow_getHeight(static_cast<ANativeWindow *>(window))));
#endif
        const auto              &fn  = device.GetVulkanInstance().GetInstanceFn();
        const auto               gpu = device.GetGpuHandle();
        VkSurfaceCapabilitiesKHR caps{};
        auto                     result = fn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);
        if (result != VK_SUCCESS)
            return result;
        if (!caps.maxImageExtent.width || !caps.maxImageExtent.height)
            return VK_NOT_READY;
        ci         = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        ci.surface = surface;
        if (caps.currentExtent.width != UINT32_MAX) {
            ci.imageExtent = caps.currentExtent;
        } else {
            if (!width || !height)
                return VK_NOT_READY;
            ci.imageExtent = {std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width),
                              std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height)};
        }
        if (!ci.imageExtent.width || !ci.imageExtent.height)
            return VK_NOT_READY;
        ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if ((caps.supportedUsageFlags & ci.imageUsage) != ci.imageUsage)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        const auto transform = static_cast<uint32_t>(caps.currentTransform);
        if (!transform || (transform & (transform - 1)) || transform > VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR ||
            !(caps.supportedTransforms & transform))
            return VK_ERROR_FEATURE_NOT_PRESENT;
        ci.preTransform = caps.currentTransform;
        for (const auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                 VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
            if (caps.supportedCompositeAlpha & alpha) {
                ci.compositeAlpha = alpha;
                break;
            }
        if (!ci.compositeAlpha || caps.minImageCount == UINT32_MAX)
            return VK_ERROR_INITIALIZATION_FAILED;
        ci.minImageCount = caps.minImageCount + 1;
        if (caps.maxImageCount)
            ci.minImageCount = std::min(ci.minImageCount, caps.maxImageCount);
        ci.imageArrayLayers = 1;
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.clipped          = VK_TRUE;
        return VK_SUCCESS;
    }

    VkResult VulkanSwapChain::QueryPresentationOptions(VkSwapchainCreateInfoKHR &ci) const
    {
        const auto &fn     = device.GetVulkanInstance().GetInstanceFn();
        const auto  gpu    = device.GetGpuHandle();
        uint32_t    count  = 0;
        auto        result = fn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
        if (result != VK_SUCCESS)
            return result;
        if (!count || count > 1024)
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        std::vector<VkSurfaceFormatKHR> formats(count);
        result = fn.vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());
        if (result != VK_SUCCESS)
            return result == VK_INCOMPLETE ? VK_NOT_READY : result;
        formats.resize(count);
        std::sort(formats.begin(), formats.end(),
                  [](const auto &a, const auto &b) { return a.format != b.format ? a.format < b.format : a.colorSpace < b.colorSpace; });
        const auto supported = [&](const auto &f) { return ToPixelFormat(f.format) != PixelFormat::UNDEFINED; };
        auto       selected  = std::find_if(formats.begin(), formats.end(), supported);
        if (selected == formats.end())
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        const auto current = std::find_if(formats.begin(), formats.end(),
                                          [&](const auto &f) { return f.format == applied.imageFormat && f.colorSpace == applied.imageColorSpace; });
        if (current != formats.end())
            selected = current;
        const auto preferred = std::find_if(formats.begin(), formats.end(), [&](const auto &f) {
            return f.format == FromPixelFormat(preferredFormat) && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (preferred != formats.end())
            selected = preferred;
        ci.imageFormat     = selected->format;
        ci.imageColorSpace = selected->colorSpace;
        result             = fn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
        if (result != VK_SUCCESS)
            return result;
        if (!count || count > 1024)
            return VK_ERROR_INITIALIZATION_FAILED;
        std::vector<VkPresentModeKHR> modes(count);
        result = fn.vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, modes.data());
        if (result != VK_SUCCESS)
            return result == VK_INCOMPLETE ? VK_NOT_READY : result;
        modes.resize(count);
        const auto desired = preferredMode == PresentMode::VSYNC ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        ci.presentMode     = std::find(modes.begin(), modes.end(), desired) != modes.end() ? desired : VK_PRESENT_MODE_FIFO_KHR;
        if (std::find(modes.begin(), modes.end(), ci.presentMode) == modes.end())
            return VK_ERROR_INITIALIZATION_FAILED;
        return VK_SUCCESS;
    }

    VkResult VulkanSwapChain::QueryConfiguration(uint32_t width, uint32_t height, VkSwapchainCreateInfoKHR &ci) const
    {
        const auto result = QueryCapabilities(width, height, ci);
        return result == VK_SUCCESS ? QueryPresentationOptions(ci) : result;
    }

    bool VulkanSwapChain::CreateSwapchain(uint32_t width, uint32_t height)
    {
        if (surfaceRevision == UINT64_MAX) {
            creationResult = VK_ERROR_TOO_MANY_OBJECTS;
            return false;
        }
        VkSwapchainCreateInfoKHR ci{};
        creationResult = QueryConfiguration(width, height, ci);
        if (creationResult != VK_SUCCESS)
            return false;
        const auto &devFn = device.GetDeviceFn();
        surfaceFormat     = {ci.imageFormat, ci.imageColorSpace};
        extent            = ci.imageExtent;
        presentMode       = ci.presentMode;
        pixelFormat       = ToPixelFormat(ci.imageFormat);
        const VkResult r  = devFn.vkCreateSwapchainKHR(device.GetNativeHandle(), &ci, nullptr, &swapchain);
        creationResult    = r;
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateSwapchainKHR failed: %d", r);
            return false;
        }

        uint32_t imageCount = 0;
        if ((creationResult = devFn.vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &imageCount, nullptr)) != VK_SUCCESS)
            return false;
        std::vector<VkImage> swapImages(imageCount);
        if ((creationResult = devFn.vkGetSwapchainImagesKHR(device.GetNativeHandle(), swapchain, &imageCount, swapImages.data())) != VK_SUCCESS)
            return false;

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
        applied = ci;
        presentationOptionsDirty = false;
        ++surfaceRevision;
        status = SwapChainStatus::OK;
        return true;
    }

    void VulkanSwapChain::DestroySwapchain()
    {
        images.clear(); // release borrowed wrappers (do not vkDestroyImage)
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
        if (checkedProtocol)
            return INVALID_INDEX;
        directProtocol     = true;
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
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            status = SwapChainStatus::OUT_OF_DATE;
            return INVALID_INDEX;
        }
        if (r == VK_ERROR_SURFACE_LOST_KHR || r == VK_ERROR_DEVICE_LOST) {
            // The window/surface is gone; mark the swapchain lost so the caller
            // stops acquiring instead of blocking on a dead surface.
            status = SwapChainStatus::LOST;
            return INVALID_INDEX;
        }
        if (r == VK_TIMEOUT) {
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
        if (checkedProtocol)
            return;
        directProtocol = true;
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
        if (r == VK_ERROR_SURFACE_LOST_KHR) {
            status = SwapChainStatus::LOST;
        } else if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            status = SwapChainStatus::OUT_OF_DATE;
        } else if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            LOG_E(TAG, "vkQueuePresentKHR failed: %d", r);
        }
    }

    SwapChainStatus VulkanSwapChain::GetStatus() const
    {
        if (status == SwapChainStatus::LOST) {
            return SwapChainStatus::LOST;
        }
        const Extent2D surfaceSize = GetSurfaceSize();
        if (surfaceSize.width != extent.width || surfaceSize.height != extent.height) {
            return SwapChainStatus::OUT_OF_DATE;
        }
        return status;
    }

    Extent2D VulkanSwapChain::GetSurfaceSize() const
    {
        VulkanInstance &inst = device.GetVulkanInstance();
        const auto &instFn   = inst.GetInstanceFn();

        VkSurfaceCapabilitiesKHR caps = {};
        instFn.vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.GetGpuHandle(), surface, &caps);
        if (caps.currentExtent.width != UINT32_MAX) {
            return {caps.currentExtent.width, caps.currentExtent.height};
        }
        return {extent.width, extent.height};
    }

    void VulkanSwapChain::Resize(uint32_t width, uint32_t height)
    {
        if (checkedProtocol)
            return;
        directProtocol = true;
        // VulkanDevice::WaitIdle is private; use the device-fn directly.
        device.GetDeviceFn().vkDeviceWaitIdle(device.GetNativeHandle());
        DestroySwapchain();
        // preserve the surface; recreate swapchain only
        CreateSwapchain(width, height);
    }

    Image *VulkanSwapChain::GetImage(uint32_t index) const
    {
        if (index >= images.size()) {
            return nullptr;
        }
        return images[index].get();
    }

    namespace {
        PresentationResult PresentationResultFromVk(VkResult result)
        {
            if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
                return {PresentationStatus::READY};
            if (result == VK_TIMEOUT || result == VK_NOT_READY)
                return {PresentationStatus::BUSY};
            if (result == VK_ERROR_OUT_OF_DATE_KHR)
                return {PresentationStatus::OUT_OF_DATE, {GpuErrorCode::NATIVE_ERROR, result}};
            if (result == VK_ERROR_SURFACE_LOST_KHR)
                return {PresentationStatus::SURFACE_LOST, {GpuErrorCode::NATIVE_ERROR, result}};
            const auto code = result == VK_ERROR_FEATURE_NOT_PRESENT || result == VK_ERROR_FORMAT_NOT_SUPPORTED  ? GpuErrorCode::UNSUPPORTED
                              : result == VK_ERROR_DEVICE_LOST                                                   ? GpuErrorCode::DEVICE_LOST
                              : result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY ? GpuErrorCode::OUT_OF_MEMORY
                                                                                                                 : GpuErrorCode::NATIVE_ERROR;
            return {PresentationStatus::FAILED, {code, result}};
        }
    } // namespace

    Device *VulkanSwapChain::GetDevice() const noexcept
    {
        return &device;
    }

    SurfaceQueryResult VulkanSwapChain::QuerySurface() const noexcept
    {
        if (status == SwapChainStatus::LOST || !surface)
            return {{PresentationStatus::SURFACE_LOST}, {}};
        VkSwapchainCreateInfoKHR desired{};
        auto                     result = QueryCapabilities(0, 0, desired);
        if (result != VK_SUCCESS)
            return {PresentationResultFromVk(result), {}};
        desired.imageFormat     = applied.imageFormat;
        desired.imageColorSpace = applied.imageColorSpace;
        desired.presentMode     = applied.presentMode;
        if (directProtocol || presentationOptionsDirty || status == SwapChainStatus::OUT_OF_DATE || !swapchain) {
            result = QueryPresentationOptions(desired);
            if (result != VK_SUCCESS)
                return {PresentationResultFromVk(result), {}};
            // Keep differing options dirty so later queries cannot silently fall back to the applied options.
            presentationOptionsDirty = desired.imageFormat != applied.imageFormat || desired.imageColorSpace != applied.imageColorSpace ||
                                       desired.presentMode != applied.presentMode;
        }
        uint8_t rotation = 0;
        for (auto transform = static_cast<uint32_t>(desired.preTransform); transform > 1; transform >>= 1)
            ++rotation;
        SurfaceConfiguration configuration{{desired.imageExtent.width, desired.imageExtent.height},
                                           ToPixelFormat(desired.imageFormat),
                                           static_cast<SurfaceTransform>(rotation),
                                           surfaceRevision};
        const bool           same = swapchain && status == SwapChainStatus::OK && desired.surface == applied.surface &&
                          desired.imageExtent.width == applied.imageExtent.width && desired.imageExtent.height == applied.imageExtent.height &&
                          desired.imageFormat == applied.imageFormat && desired.imageColorSpace == applied.imageColorSpace &&
                          desired.preTransform == applied.preTransform && desired.compositeAlpha == applied.compositeAlpha &&
                          desired.presentMode == applied.presentMode && desired.minImageCount == applied.minImageCount &&
                          desired.imageUsage == applied.imageUsage;
        return {{same ? PresentationStatus::READY : PresentationStatus::OUT_OF_DATE}, configuration};
    }

    SwapChainAcquireResult VulkanSwapChain::TryAcquireFrame(uint64_t timeoutNs) noexcept
    {
        if (!device.HasSwapchainMaintenance() || !device.GetDeviceFn().vkReleaseSwapchainImagesEXT)
            return {{PresentationStatus::FAILED, {GpuErrorCode::UNSUPPORTED}}, {}};
        if (directProtocol || timeoutNs > 1'000'000 || presentationSerial == UINT64_MAX)
            return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
        checkedProtocol = true;
        if (status == SwapChainStatus::LOST || !surface || !swapchain)
            return {{PresentationStatus::SURFACE_LOST}, {}};
        if (status == SwapChainStatus::OUT_OF_DATE)
            return {{PresentationStatus::OUT_OF_DATE}, {}};
        // FrameScheduler validates live capabilities and invalidated options after acquisition, before exposing the image.
        auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return !frame.serial; });
        if (found == presentationFrames.end()) {
            if (presentationFrames.size() >= images.size())
                return {{PresentationStatus::BUSY}, {}};
            presentationFrames.emplace_back();
            found = presentationFrames.end() - 1;
        }
        auto &frame = *found;
        if (!frame.acquired)
            frame.acquired = SemaphorePtr(device.CreateSema({}));
        if (!frame.rendered)
            frame.rendered = SemaphorePtr(device.CreateSema({}));
        if (!frame.acquireFence)
            frame.acquireFence = FencePtr(device.CreateFence({false}));
        if (!frame.presentFence)
            frame.presentFence = FencePtr(device.CreateFence({false}));
        if (!frame.acquired || !frame.rendered || !frame.acquireFence || !frame.presentFence)
            return {{PresentationStatus::FAILED, {GpuErrorCode::NATIVE_ERROR}}, {}};
        for (const auto &fence : {frame.acquireFence, frame.presentFence}) {
            const auto reset = fence->TryReset();
            if (!reset.Succeeded())
                return {{PresentationStatus::FAILED, reset.error}, {}};
        }
        const auto result = device.GetDeviceFn().vkAcquireNextImageKHR(
            device.GetNativeHandle(), swapchain, timeoutNs, static_cast<VulkanSemaphore *>(frame.acquired.Get())->GetNativeHandle(),
            static_cast<VulkanFence *>(frame.acquireFence.Get())->GetNativeHandle(), &frame.imageIndex);
        if (result == VK_SUBOPTIMAL_KHR)
            presentationOptionsDirty = true;
        const auto checked = PresentationResultFromVk(result);
        if (checked.status != PresentationStatus::READY) {
            if (checked.status == PresentationStatus::OUT_OF_DATE)
                status = SwapChainStatus::OUT_OF_DATE;
            if (checked.status == PresentationStatus::SURFACE_LOST)
                status = SwapChainStatus::LOST;
            return {checked, {}};
        }
        frame.serial    = ++presentationSerial;
        frame.presented = false;
        frame.error     = {};
        return {checked, {frame.serial, frame.imageIndex, frame.acquired.Get(), frame.rendered.Get()}};
    }

    PresentationResult VulkanSwapChain::TryPresentFrame(uint64_t serial, bool /*gpuComplete*/) noexcept
    {
        const auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(),
                                        [serial](const auto &frame) { return serial && frame.serial == serial; });
        if (found == presentationFrames.end() || found->presented)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        auto                          &frame     = *found;
        const auto                     fence     = static_cast<VulkanFence *>(frame.presentFence.Get())->GetNativeHandle();
        const auto                     semaphore = static_cast<VulkanSemaphore *>(frame.rendered.Get())->GetNativeHandle();
        VkSwapchainPresentFenceInfoEXT completion{VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT};
        completion.swapchainCount = 1;
        completion.pFences        = &fence;
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        present.pNext              = &completion;
        present.swapchainCount     = 1;
        present.pSwapchains        = &swapchain;
        present.pImageIndices      = &frame.imageIndex;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &semaphore;
        const auto result =
            device.GetDeviceFn().vkQueuePresentKHR(static_cast<VulkanQueue *>(device.GetQueue(QueueType::GRAPHICS))->GetNativeHandle(), &present);
        if (result == VK_SUBOPTIMAL_KHR)
            presentationOptionsDirty = true;
        auto checked = PresentationResultFromVk(result);
        if (checked.error.code == GpuErrorCode::OUT_OF_MEMORY) {
            checked.status = PresentationStatus::BUSY;
            return checked;
        }
        frame.presented = checked.status != PresentationStatus::FAILED;
        if (checked.status == PresentationStatus::FAILED)
            frame.error = checked.error;
        if (checked.status == PresentationStatus::SURFACE_LOST)
            status = SwapChainStatus::LOST;
        if (checked.status == PresentationStatus::OUT_OF_DATE)
            status = SwapChainStatus::OUT_OF_DATE;
        return checked;
    }

    FenceQueryResult VulkanSwapChain::RetireFrame(uint64_t serial, bool abandon) noexcept
    {
        const auto found = std::find_if(presentationFrames.begin(), presentationFrames.end(),
                                        [serial](const auto &frame) { return serial && frame.serial == serial; });
        if (found == presentationFrames.end())
            return {FenceStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        auto &frame = *found;
        if (frame.error.code != GpuErrorCode::NONE)
            return {FenceStatus::FAILED, frame.error};
        if (frame.presented == abandon)
            return {FenceStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        const auto acquired = frame.acquireFence->Query();
        if (acquired.status != FenceStatus::SIGNALED)
            return acquired;
        const auto completed = abandon ? acquired : frame.presentFence->Query();
        if (completed.status != FenceStatus::SIGNALED)
            return completed;
        if (abandon) {
            VkReleaseSwapchainImagesInfoEXT release{VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT};
            release.swapchain       = swapchain;
            release.imageIndexCount = 1;
            release.pImageIndices   = &frame.imageIndex;
            const auto result       = device.GetDeviceFn().vkReleaseSwapchainImagesEXT(device.GetNativeHandle(), &release);
            if (result != VK_SUCCESS)
                return {FenceStatus::FAILED, PresentationResultFromVk(result).error};
            // Abandoned binary signals have no consumer; destroy them after acquire/GPU completion.
            frame.acquired = {};
            frame.rendered = {};
        }
        frame.serial = 0;
        return {FenceStatus::SIGNALED};
    }

    PresentationResult VulkanSwapChain::TryResize(uint32_t width, uint32_t height) noexcept
    {
        if (directProtocol)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (!width || !height || std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }))
            return {PresentationStatus::BUSY};
        if (status == SwapChainStatus::LOST)
            return {PresentationStatus::SURFACE_LOST};
        presentationFrames.clear();
        DestroySwapchain();
        if (!CreateSwapchain(width, height))
            return PresentationResultFromVk(creationResult);
        return {PresentationStatus::READY};
    }

    PresentationResult VulkanSwapChain::TryClose() noexcept
    {
        if (directProtocol)
            return {PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        checkedProtocol = true;
        if (std::any_of(presentationFrames.begin(), presentationFrames.end(), [](const auto &frame) { return frame.serial; }))
            return {PresentationStatus::BUSY};
        presentationFrames.clear();
        DestroySwapchain();
        DestroySurface();
        status = SwapChainStatus::LOST;
        return {PresentationStatus::READY};
    }
} // namespace sky::aurora
