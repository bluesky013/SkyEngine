//
// Created by Zach Lee on 2022/11/10.
//

#include <rhi/Instance.h>
#include <unordered_map>
#include <string>

namespace sky::rhi {
    using Func = Instance*(*)();

    Instance *Instance::Create(const Descriptor &desc)
    {
        std::string nameMap[] = {
            "VULKANRHI",
            "VULKANRHI",
            "METALRHI",
            "DX12RHI",
            "GLESRHI",
        };

        auto api = nameMap[static_cast<uint32_t>(desc.api)];
        auto module = std::make_unique<DynamicModule>(api);
        if (!module->Load()) {
            return false;
        }
        auto instanceFunc = reinterpret_cast<Func>(module->GetAddress("CreateInstance"));
        if (instanceFunc == nullptr) {
            return false;
        }
        auto instance = instanceFunc();
        if (instance != nullptr && instance->Init(desc)) {
            instance->module = std::move(module);
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
