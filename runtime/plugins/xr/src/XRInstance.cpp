//
// Created by blues on 2024/2/28.
//

#include <xr/XRInstance.h>
#include <core/logger/Logger.h>

#include <framework/platform/PlatformBase.h>
#include <render/RHI.h>
#include <openxr/openxr_platform.h>

static const char* TAG = "XRInstance";
namespace sky {

    void XRInstance::AddFunctions(const std::string_view &name)
    {
        PFN_xrVoidFunction func = nullptr;
        xrGetInstanceProcAddr(instance, name.data(), &func);
        if (func != nullptr) {
            functionMap.emplace(name, func);
        }
    }

    void XRInstance::InitFunctions()
    {
        if (api == rhi::API::VULKAN) {
            AddFunctions("xrGetVulkanGraphicsRequirementsKHR");
            AddFunctions("xrGetVulkanGraphicsDeviceKHR");
            AddFunctions("xrGetVulkanInstanceExtensionsKHR");
            AddFunctions("xrGetVulkanDeviceExtensionsKHR");
        }
        AddFunctions("xrEnumerateSwapchainImages");
    }

    bool XRInstance::Init(const Desc &desc)
    {
        api = desc.api;

        options.viewCfgType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        options.formFactor  = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        options.blendMode   = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

        std::vector<const char*> xrInstanceExt;
        if (api == rhi::API::VULKAN) {
            xrInstanceExt.emplace_back("XR_KHR_vulkan_enable");
        }

        platform.reset(CreateXRPlatform());
        uint32_t count;
        XR_CHECK_RESULT(xrEnumerateApiLayerProperties(0, &count, nullptr));
        std::vector<XrApiLayerProperties> layers(count, {XR_TYPE_API_LAYER_PROPERTIES});
        XR_CHECK_RESULT(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &count, layers.data()));

        for (auto &layer : layers) {
            LOG_I(TAG, "XR Layer %s\t, %s", layer.layerName, layer.description);
        }

        XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(nullptr, 0, &count, nullptr));
        std::vector<XrExtensionProperties> extensions(count, {XR_TYPE_EXTENSION_PROPERTIES});
        XR_CHECK_RESULT(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)extensions.size(), &count, extensions.data()));

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        createInfo.next = platform->GetInstanceCreateInfo();
        createInfo.enabledExtensionCount = (uint32_t)xrInstanceExt.size();
        createInfo.enabledExtensionNames = xrInstanceExt.data();

        createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
        sprintf(createInfo.applicationInfo.applicationName, "%s", "XRModule");
        sprintf(createInfo.applicationInfo.engineName, "%s", "SkyEngine");

        XrResult res = xrCreateInstance(&createInfo, &instance);
        if (res != XR_SUCCESS) {
            return false;
        }

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = options.formFactor;

        res = xrGetSystem(instance, &systemInfo, &systemId);
        if (res != XR_SUCCESS) {
            return false;
        }

        XR_CHECK_RESULT(xrEnumerateEnvironmentBlendModes(instance, systemId, options.viewCfgType, 0, &count, nullptr));
        std::vector<XrEnvironmentBlendMode> blendModes(count);
        XR_CHECK_RESULT(xrEnumerateEnvironmentBlendModes(instance, systemId, options.viewCfgType, count, &count, blendModes.data()));

        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        xrGetInstanceProperties(instance, &instanceProperties);
        LOG_I(TAG, "XR instance info. %s, version %llu", instanceProperties.runtimeName, instanceProperties.runtimeVersion);

        InitFunctions();

        RHI::Get()->SetXRInterface(this);

        LOG_I(TAG, "XR_RUNTIME_JSON: %s", Platform::Get()->GetEnvVariable("XR_RUNTIME_JSON").c_str());
        return true;
    }

    const XrEventDataBaseHeader *XRInstance::FetchEvent()
    {
        auto *baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&eventDataBuffer);
        *baseHeader = {XR_TYPE_EVENT_DATA_BUFFER};
        const XrResult xr = xrPollEvent(instance, &eventDataBuffer);
        if (xr == XR_SUCCESS) {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const auto* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                LOG_W(TAG, "%d events lost", eventsLost->lostEventCount);
            }
            return baseHeader;
        }
        return nullptr;
    }

    void XRInstance::PollEvents()
    {
        while (const auto *event = FetchEvent()) {
            switch (event->type) {
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
                    const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
                    LOG_W(TAG, "XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime);
                    return;
                }
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                    session->SessionStateHandler(sessionStateChangedEvent);
                    break;
                }
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                default: {
                    LOG_W(TAG, "Ignoring event type %d", event->type);
                    break;
                }
            }
        }
    }

    void XRInstance::BeginFrame()
    {
        PollEvents();

        if (session->IsRunning()) {
            session->PollActions();
            session->BeginFrame();
        }
    }

    void XRInstance::EndFrame()
    {
        if (session->IsRunning()) {
            std::vector<const XrCompositionLayerBaseHeader*> layers;

            if (session->FrameState().shouldRender == XR_TRUE) {
                layers.emplace_back(reinterpret_cast<const XrCompositionLayerBaseHeader*>(&swapChain->GetLayerProjection()));
            }

            XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
            frameEndInfo.displayTime = session->FrameState().predictedDisplayTime;
            frameEndInfo.environmentBlendMode = options.blendMode;
            frameEndInfo.layerCount = static_cast<uint32_t>(layers.size());
            frameEndInfo.layers = layers.data();

            session->EndFrame(frameEndInfo);
        }
    }

    rhi::IXRSwapChain* XRInstance::CreateXrSwapChain(const rhi::IXRSwapChain::Descriptor &desc)
    {
        if (!swapChain) {
            swapChain = std::make_unique<XRSwapChain>(*session);
            swapChain->Init(desc);
        }
        return swapChain.get();
    }

    void XRInstance::DestroyXrSwapChain(rhi::IXRSwapChain *swc)
    {
        SKY_ASSERT(swapChain.get() == swc);
        swapChain = nullptr;
    }

    PFN_xrVoidFunction XRInstance::GetFunction(const std::string_view &func) const
    {
        auto iter = functionMap.find(func);
        return iter != functionMap.end() ? iter->second : nullptr;
    }

    void XRInstance::SetSessionGraphicsBinding(const void *data)
    {
        // init session
        session = std::make_unique<XRSession>(*this);
        session->Init(data);
    }

} // namespace sky