//
// Created by blues on 2026/3/29.
//

#include <aurora/rhi/Instance.h>

namespace sky::aurora {

    using InstanceFunc = Instance::Impl *(*)();

    Instance::~Instance()
    {
        Reset();
    }

    void Instance::Reset()
    {
        if (device != nullptr) {
            device->Shutdown();
            delete device;
        }
        device    = nullptr;
        impl      = nullptr;
        dynModule = nullptr;
    }

    void Instance::Init(const Descriptor &desc)
    {
        std::lock_guard lock(mutex);
        if (impl || claimed) {
            return;
        }
        Load(desc);
    }

    bool Instance::Load(const Descriptor &desc)
    {
        if (static_cast<uint32_t>(desc.api) > static_cast<uint32_t>(API::DX12))
            return false;

        std::string nameMap[] = {
#ifdef __APPLE__
            "Aurora.Metal",
#else
            "Aurora.Vulkan",
#endif
            "Aurora.Vulkan",
            "Aurora.Metal",
            "Aurora.DX12",
        };

        auto api  = nameMap[static_cast<uint32_t>(desc.api)];
        dynModule = std::make_unique<DynamicModule>(api);
        if (!dynModule->Load()) {
            Reset();
            return false;
        }
        auto instanceFunc = reinterpret_cast<InstanceFunc>(dynModule->GetAddress("CreateInstance"));
        if (instanceFunc == nullptr) {
            Reset();
            return false;
        }
        impl.reset(instanceFunc());
        if (impl && !impl->Init(desc)) {
            Reset();
            return false;
        }

        device = impl ? impl->CreateDevice() : nullptr;
        if (!device)
            Reset();
        return device != nullptr;
    }

    Instance::DeviceSession::DeviceSession(Instance &instance, uint64_t value) : owner(instance), epoch(value)
    {
    }
    Instance::DeviceSession::~DeviceSession()
    {
        std::lock_guard lock(owner.mutex);
        owner.Reset();
        owner.claimed = false;
    }
    Device *Instance::DeviceSession::GetDevice() const noexcept
    {
        return owner.device;
    }
    uint64_t Instance::DeviceSession::GetEpoch() const noexcept
    {
        return epoch;
    }
    std::variant<GpuError, std::unique_ptr<Instance::DeviceSession>> Instance::Acquire(const Descriptor &desc)
    {
        std::lock_guard lock(mutex);
        if (claimed || device || impl)
            return GpuError{GpuErrorCode::INVALID_ARGUMENT};
        if (!nextEpoch || !Load(desc))
            return GpuError{GpuErrorCode::UNSUPPORTED};
        claimed = true;
        return std::unique_ptr<DeviceSession>(new DeviceSession(*this, nextEpoch++));
    }

} // namespace sky::aurora
