//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Device.h>
#include <mtl/Instance.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    bool Device::Init(const Descriptor &des)
    {
        const auto &mtlDevices = instance.GetMtlDevices();
        device = mtlDevices.front();

        queues.resize(2);
        queues[0] = std::unique_ptr<Queue>(new Queue(*this));
        queues[0]->StartThread();
        graphicsQueue = queues[0].get();

        queues[1] = std::unique_ptr<Queue>(new Queue(*this));
        queues[1]->StartThread();
        transferQueue = queues[1].get();

        return true;
    }

    rhi::Queue* Device::GetQueue(rhi::QueueType type) const
    {
        return type == rhi::QueueType::GRAPHICS ? graphicsQueue : transferQueue;
    }
}
