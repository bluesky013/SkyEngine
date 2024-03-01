//
// Created by blues on 2024/3/11.
//

#include <xr/XRSwapchain.h>
#include <xr/XRCore.h>
#include <xr/XRSession.h>
#include <xr/XRInstance.h>
#include <openxr/xr_linear.h>

namespace sky {

    XRSwapChain::~XRSwapChain()
    {
        if (swapChain != XR_NULL_HANDLE) {
            XR_CHECK_RESULT(xrDestroySwapchain(swapChain));
        }
    }

    void XRSwapChain::Init(const Descriptor &desc)
    {
        auto &instance = session.GetInstance();

        XrInstance instHandle = instance.GetXrInstanceHandle();
        XrSystemId systemId = instance.GetXrSystemId();
        XrSession sessionId = session.GetHandle();

        XR_CHECK_RESULT(xrEnumerateViewConfigurationViews(instHandle, systemId, instance.GetOptions().viewCfgType, 0, &viewCount, nullptr));
        std::vector<XrViewConfigurationView> configViews(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        XR_CHECK_RESULT(xrEnumerateViewConfigurationViews(instHandle, systemId, instance.GetOptions().viewCfgType, viewCount, &viewCount, configViews.data()));

        uint32_t formatCount;
        XR_CHECK_RESULT(xrEnumerateSwapchainFormats(sessionId, 0, &formatCount, nullptr));
        std::vector<int64_t> formats(formatCount);
        XR_CHECK_RESULT(xrEnumerateSwapchainFormats(sessionId, formatCount, &formatCount, formats.data()));

        auto iter = std::find(formats.begin(), formats.end(), desc.format);

        width = configViews[0].recommendedImageRectWidth;
        height = configViews[0].recommendedImageRectHeight;
        format = iter != formats.end() ? *iter : formats[0];
        views.resize(viewCount);

        XrSwapchainCreateInfo swcCreateInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swcCreateInfo.arraySize = viewCount;
        swcCreateInfo.format = format;
        swcCreateInfo.width = width;
        swcCreateInfo.height = height;
        swcCreateInfo.mipCount = 1;
        swcCreateInfo.faceCount = 1;
        swcCreateInfo.sampleCount = 1;
        swcCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        XR_CHECK_RESULT(xrCreateSwapchain(sessionId, &swcCreateInfo, &swapChain));
    }

    uint32_t XRSwapChain::AcquireNextImage()
    {
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        XR_CHECK_RESULT(xrAcquireSwapchainImage(swapChain, &acquireInfo, &imageIndex));

        XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = XR_INFINITE_DURATION;
        XR_CHECK_RESULT(xrWaitSwapchainImage(swapChain, &waitInfo));

        return imageIndex;
    }

    bool XRSwapChain::RequestViewData(const rhi::XRViewInput &input, std::vector<rhi::XRViewData> &data)
    {
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        uint32_t viewCapacityInput = viewCount;
        uint32_t viewCountOutput = 0;

        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = session.GetInstance().GetOptions().viewCfgType;
        viewLocateInfo.displayTime = session.FrameState().predictedDisplayTime;
        viewLocateInfo.space = session.GetSpace();

        if (session.FrameState().shouldRender != XR_TRUE) {
            return false;
        }

        auto result = xrLocateViews(session.GetHandle(), &viewLocateInfo, &viewState, viewCapacityInput, &viewCountOutput, views.data());
        if (result != XR_SUCCESS) {
            return false;
        }

        data.resize(viewCount);
        layerViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});
        for (uint32_t i = 0; i < viewCount; ++i) {
            const auto& pose = views[i].pose;
            const auto& fov = views[i].fov;

            auto &proj = reinterpret_cast<XrMatrix4x4f&>(data[i].project);
            XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, fov, input.vNear, input.vFar);
            auto &world = reinterpret_cast<XrMatrix4x4f&>(data[i].world);
            XrVector3f scale{1.f, 1.f, 1.f};
            XrMatrix4x4f_CreateTranslationRotationScale(&world, &pose.position, &pose.orientation, &scale);

            layerViews[i].pose = pose;
            layerViews[i].fov = fov;
            layerViews[i].subImage.swapchain = swapChain;
            layerViews[i].subImage.imageRect.offset = {0, 0};
            layerViews[i].subImage.imageRect.extent = {static_cast<int32_t>(width), static_cast<int32_t>(height)};
            layerViews[i].subImage.imageArrayIndex = i;
        }

        layer.space = viewLocateInfo.space;
        layer.layerFlags =
                session.GetInstance().GetOptions().blendMode == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
                ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
                : 0;
        layer.viewCount = static_cast<uint32_t>(layerViews.size());
        layer.views = layerViews.data();

        return true;
    }

    void XRSwapChain::Present()
    {
        XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        XR_CHECK_RESULT(xrReleaseSwapchainImage(swapChain, &releaseInfo));
    }

} // namespace sky