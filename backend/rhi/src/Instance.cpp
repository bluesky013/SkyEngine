//
// Created by Zach Lee on 2022/11/10.
//

#include <rhi/Instance.h>
#include <unordered_map>
#include <string>

std::unique_ptr<sky::DynamicModule> g_RHI;

namespace sky::rhi {
    using Func = Instance*(*)();

    Instance *Instance::Create(const Descriptor &desc)
    {
        std::string nameMap[] = {
            "VULKANRHI",
            "VulkanRHI",
            "METALRHI",
            "DX12RHI",
            "GLESRHI",
        };

        auto api = nameMap[static_cast<uint32_t>(desc.api)];
        g_RHI = std::make_unique<DynamicModule>(api);
        if (!g_RHI->Load()) {
            return false;
        }
        auto instanceFunc = reinterpret_cast<Func>(g_RHI->GetAddress("CreateInstance"));
        if (instanceFunc == nullptr) {
            return false;
        }
        auto instance = instanceFunc();
        if (instance != nullptr && instance->Init(desc)) {
            return instance;
        }
        if (instance != nullptr) {
            delete instance;
        }
        return nullptr;
    }

    void Instance::Destroy(Instance *instance)
    {
        if (instance != nullptr) {
            delete instance;
        }
    }

}
