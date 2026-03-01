//
// Created by blues on 2024/2/28.
//

#pragma once

#include <openxr/openxr.h>
#include <xr/IXRPlatform.h>
#include <xr/XRSession.h>
#include <xr/XRCore.h>
#include <xr/XRSwapchain.h>

#include <rhi/XRInterface.h>
#include <rhi/Instance.h>

#include <unordered_map>

namespace sky {

    struct XROption {
        XrFormFactor formFactor;
        XrViewConfigurationType viewCfgType;
        XrEnvironmentBlendMode blendMode;
    };

    class XRInstance : public rhi::XRInterface {
    public:
        XRInstance() = default;
        ~XRInstance() override = default;

        struct Desc {
            rhi::API api;
        };

        bool Init(const Desc &desc);

        void BeginFrame();
        void EndFrame();

        XrInstance GetXrInstanceHandle() const override { return instance; }
        XrSystemId GetXrSystemId() const override { return systemId; }
        rhi::IXRSwapChain* CreateXrSwapChain(const rhi::IXRSwapChain::Descriptor &desc) override;
        void DestroyXrSwapChain(rhi::IXRSwapChain *) override;

        const XROption &GetOptions() const { return options; }
    private:
        void InitFunctions();
        void AddFunctions(const std::string_view &name);
        void PollEvents();
        const XrEventDataBaseHeader *FetchEvent();

        PFN_xrVoidFunction GetFunction(const std::string_view &func) const override;
        void SetSessionGraphicsBinding(const void *data) override;

        rhi::API api = rhi::API::VULKAN;
        XrInstance instance = XR_NULL_HANDLE;
        XrSystemId systemId = XR_NULL_SYSTEM_ID;

        std::unique_ptr<IXRPlatform> platform;
        std::unique_ptr<XRSession> session;

        std::unordered_map<std::string_view, PFN_xrVoidFunction> functionMap;
        XrEventDataBuffer eventDataBuffer{XR_TYPE_EVENT_DATA_BUFFER};

        XROption options;

        std::unique_ptr<XRSwapChain> swapChain;
    };

} // namespace sky