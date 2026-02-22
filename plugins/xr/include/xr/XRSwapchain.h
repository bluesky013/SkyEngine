//
// Created by blues on 2024/3/11.
//

#pragma once

#include <openxr/openxr.h>
#include <rhi/XRInterface.h>

namespace sky {
    class XRSession;

    class XRSwapChain : public rhi::IXRSwapChain {
    public:
        explicit XRSwapChain(XRSession &session_) : session(session_) {}
        ~XRSwapChain() override;

        void Init(const Descriptor &desc);

        uint32_t AcquireNextImage() override;
        void Present() override;

        bool RequestViewData(const rhi::XRViewInput &input, std::vector<rhi::XRViewData> &data) override;
        XrSwapchain GetHandle(uint32_t index) const override { return swapChain; }
        uint32_t GetWidth() const override { return width; }
        uint32_t GetHeight() const override { return height; }
        uint32_t GetArrayLayers() const override { return viewCount; }
        int64_t GetFormat() const override { return format; }

        const XrCompositionLayerProjection &GetLayerProjection() const { return layer; }
    private:
        XRSession &session;
        XrSwapchain swapChain = XR_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t viewCount = 0;
        int64_t format = 0;

        std::vector<XrView> views;
        std::vector<XrCompositionLayerProjectionView> layerViews;
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    };


} // namespace sky