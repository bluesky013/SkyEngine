//
// Created by Zach Lee on 2022/11/10.
//

#include <rhi/Instance.h>
#include <unordered_map>
#include <string>

std::unique_ptr<sky::DynamicModule> g_RHI;

namespace sky::rhi {
    using Func = Instance*(*)();

    API GetApiByString(const std::string &rhi)
    {
        if (rhi == "gles") {
            return API::GLES;
        }
        if (rhi == "vulkan") {
            return API::VULKAN;
        }
        if (rhi == "dx12") {
            return API::DX12;
        }
        if (rhi == "metal") {
            return API::METAL;
        }
        return API::VULKAN;
    }

    Instance *Instance::Create(const Descriptor &desc)
    {
        std::string nameMap[] = {
            "VulkanRHI",
            "VulkanRHI",
            "MetalRHI",
            "DX12RHI",
            "GLESRHI",
        };

        auto api = nameMap[static_cast<uint32_t>(desc.api)];
        g_RHI = std::make_unique<DynamicModule>(api);
        if (!g_RHI->Load()) {
            return nullptr;
        }
        auto instanceFunc = reinterpret_cast<Func>(g_RHI->GetAddress("CreateInstance"));
        if (instanceFunc == nullptr) {
            return nullptr;
        }
        auto *instance = instanceFunc();
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
