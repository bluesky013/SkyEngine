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

    Device::~Device()
    {
        for (auto &queue : queues) {
            queue->Shutdown();
        }
        queues.clear();

        if (sharedEventListener != nullptr) {
            [sharedEventListener release];
            sharedEventListener = nil;
        }
    }

    bool Device::Init(const Descriptor &des)
    {
        const auto &mtlDevices = instance.GetMtlDevices();
        device = mtlDevices.front();

        queues.resize(2);
        for (uint32_t i = 0; i < 2; ++i) {
            queues[i] = std::unique_ptr<Queue>(new Queue(*this));
            queues[i]->queue = [device newCommandQueue];
            queues[i]->StartThread();
        }
        graphicsQueue = queues[0].get();
        transferQueue = queues[1].get();

        sharedEventListener = [[MTLSharedEventListener alloc] init];
        return true;
    }

    rhi::Queue* Device::GetQueue(rhi::QueueType type) const
    {
        return type == rhi::QueueType::GRAPHICS ? graphicsQueue : transferQueue;
    }
}
