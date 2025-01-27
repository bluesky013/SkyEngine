//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/environment/Singleton.h>
#include <rhi/Device.h>
#include <rhi/Instance.h>
#include <shader/ShaderCompiler.h>

namespace sky {

    class RHI : public Singleton<RHI> {
    public:
        RHI() = default;
        ~RHI() override;

#ifdef SKY_ENABLE_XR
        void SetXRInterface(rhi::XRInterface *interface) { xrInterface = interface; }
#endif

        void InitInstance(rhi::Instance::Descriptor desc);
        void InitDevice(const rhi::Device::Descriptor &desc);

        std::string GetBackendName() const;
        ShaderCompileTarget GetShaderTarget() const { return target; }
        rhi::API GetBackend() const { return api; }
        rhi::Device *GetDevice() const { return device.get(); }

    private:
        rhi::Instance *instance = nullptr;
        rhi::API api = rhi::API::DEFAULT;
        ShaderCompileTarget target = ShaderCompileTarget::SPIRV;
#ifdef SKY_ENABLE_XR
        rhi::XRInterface *xrInterface = nullptr;
#endif
        std::unique_ptr<rhi::Device> device;
    };
} // namespace sky