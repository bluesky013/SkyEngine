//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Device.h>
#include <dx12/Instance.h>

namespace sky::dx {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    Device::~Device()
    {
        for (auto &queue : queues) {
            queue->Shutdown();
        }
        queues.clear();
    }

    bool Device::Init(const Descriptor &desc, ComPtr<IDXGIAdapter1> &adaptor)
    {
        if (FAILED(D3D12CreateDevice(adaptor.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(device.GetAddressOf())))) {
            return false;
        }

        std::vector<rhi::QueueType> queueTypes = {
            rhi::QueueType::GRAPHICS,
            rhi::QueueType::COMPUTE,
            rhi::QueueType::TRANSFER
        };

        queues.reserve(queueTypes.size());
        for (auto &type : queueTypes) {
            queues.emplace_back(new Queue(*this));
            queues.back()->Init({type});
        }
        graphicsQueue = queues[0].get();
        computeQueue = queues[1].get();
        transferQueue = queues[2].get();

        return true;
    }

    ID3D12Device1 *Device::GetDevice() const
    {
        return device.Get();
    }

    IDXGIFactory2 *Device::GetDXGIFactory() const
    {
        return instance.GetDXGIFactory();
    }

    rhi::Queue* Device::GetQueue(rhi::QueueType type) const
    {
        if (type == rhi::QueueType::COMPUTE) return computeQueue;
        if (type == rhi::QueueType::TRANSFER) return transferQueue;
        return graphicsQueue;
    }
}