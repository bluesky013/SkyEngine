//
// Created by Zach Lee on 2023/4/16.
//

#include <render/RHI.h>

namespace sky {

    RHI::~RHI()
    {
        device = nullptr;
        rhi::Instance::Destroy(instance);
        instance = nullptr;
    }

    std::string RHI::GetBackendName() const
    {
        switch (api) {
            case rhi::API::VULKAN:
                return "vulkan";
            case rhi::API::METAL:
                return "metal";
            case rhi::API::DX12:
                return "d3d12";
            case rhi::API::GLES:
                return "gles";
            default:
                break;
        }
        return "Unknown";
    }

    void RHI::InitInstance(rhi::Instance::Descriptor desc)
    {
#ifdef SKY_ENABLE_XR
        desc.xrInterface = xrInterface;
#endif
        instance = rhi::Instance::Create(desc);
        api = desc.api;

        switch (api) {
        case rhi::API::VULKAN:
            target = ShaderCompileTarget::SPIRV;
            break;
        case rhi::API::METAL:
            target = ShaderCompileTarget::MSL;
            break;
        case rhi::API::DX12:
            target = ShaderCompileTarget::DXIL;
            break;
        case rhi::API::GLES:
        case rhi::API::DEFAULT:;
            SKY_ASSERT(false);
            break;
        }
    }

    void RHI::InitDevice(const rhi::Device::Descriptor &desc)
    {
        device.reset(instance->CreateDevice(desc));
    }
}
