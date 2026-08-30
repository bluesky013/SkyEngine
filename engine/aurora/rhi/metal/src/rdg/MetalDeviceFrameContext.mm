//
// Created by Zach Lee on 2026/8/9.
//

#include <rdg/MetalDeviceFrameContext.h>
#include <MetalDevice.h>

namespace sky::aurora {

    MetalDeviceFrameContext::MetalDeviceFrameContext(MetalDevice* device, const DeviceFrameContextInitInfo& info)
        : mDevice(device)
    {
        mPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        mMetalPool = static_cast<MetalCommandPool*>(mPool.get());
        
        mBuffers.resize(info.inflightNum);
        for (uint32_t i = 0; i < info.inflightNum; ++i)
        {
            mBuffers[i] = mMetalPool->Allocate();
        }
    }

    MetalDeviceFrameContext::~MetalDeviceFrameContext() noexcept
    {
        mPool = nullptr;
        mMetalPool = nullptr;
    }
} // sky::aurora
