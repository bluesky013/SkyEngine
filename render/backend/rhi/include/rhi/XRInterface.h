//
// Created by blues on 2024/3/3.
//

#pragma once

#include <vector>
#include <string>
#include <memory>

#include <core/math/Matrix4.h>

#ifdef SKY_ENABLE_XR
#include <openxr/openxr.h>

namespace sky::rhi {
    struct XRViewInput {
        float vNear;
        float vFar;
    };
    struct XRViewData {
        Matrix4 world;
        Matrix4 project;
    };

    class IXRSwapChain {
    public:
        IXRSwapChain() = default;
        virtual ~IXRSwapChain() = default;

        struct Descriptor {
            int64_t format;
            bool enableMultiView;
        };

        virtual bool RequestViewData(const XRViewInput &input, std::vector<XRViewData> &data) = 0;
        virtual XrSwapchain GetHandle(uint32_t index) const = 0;
        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual uint32_t GetArrayLayers() const = 0;
        virtual int64_t GetFormat() const = 0;
        virtual uint32_t AcquireNextImage() = 0;
        virtual void Present() = 0;
    };

    class XRInterface {
    public:
        XRInterface() = default;
        virtual ~XRInterface() = default;

        virtual PFN_xrVoidFunction GetFunction(const std::string_view &func) const { return nullptr; }

        virtual XrInstance GetXrInstanceHandle() const { return XR_NULL_HANDLE; }
        virtual XrSystemId GetXrSystemId() const { return XR_NULL_SYSTEM_ID; }
        virtual IXRSwapChain* CreateXrSwapChain(const IXRSwapChain::Descriptor &desc) { return XR_NULL_HANDLE; }
        virtual void DestroyXrSwapChain(IXRSwapChain *) {}

        virtual void SetSessionGraphicsBinding(const void *data) {}
    };
} // namespace sky::rhi

#endif