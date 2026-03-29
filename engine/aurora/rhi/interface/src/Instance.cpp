//
// Created by blues on 2026/3/29.
//

#include <aurora/rhi/Instance.h>

namespace sky::aurora {

    using InstanceFunc = Instance::Impl*(*)();

    Instance::~Instance()
    {
        if (device != nullptr) {
            device->Shutdown();
            delete device;
        }
        device  = nullptr;
        impl = nullptr;
        dynModule = nullptr;
    }

    void Instance::Init(const Descriptor &desc)
    {
        if (impl) {
            return;
        }

        std::string nameMap[] = {
            "AuroraVulkan",
            "AuroraVulkan",
            "AuroraMetal",
            "AuroraDX12",
            "AuroraGL",
        };

        auto api = nameMap[static_cast<uint32_t>(desc.api)];
        dynModule = std::make_unique<DynamicModule>(api);
        if (!dynModule->Load()) {
            return;
        }
        auto instanceFunc = reinterpret_cast<InstanceFunc>(dynModule->GetAddress("CreateInstance"));
        if (instanceFunc == nullptr) {
            return;
        }
        impl.reset(instanceFunc());
        if (impl && !impl->Init(desc)) {
            impl = nullptr;
            return;
        }

        device = impl ? impl->CreateDevice() : nullptr;
    }

} // sky::aurora