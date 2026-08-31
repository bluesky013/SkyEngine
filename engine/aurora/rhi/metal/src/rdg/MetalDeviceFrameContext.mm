//
// Created by Zach Lee on 2026/8/9.
//

#include <rdg/MetalDeviceFrameContext.h>
#include <MetalDevice.h>

namespace sky::aurora {

    MetalDeviceFrameContext::MetalDeviceFrameContext(MetalDevice* device, const DeviceFrameContextInitInfo& info)
        : mDevice(device)
    {
        mInflightNum = info.inflightNum;
        mParallelNum = info.parallelNum;

        mPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        mMetalPool = static_cast<MetalCommandPool*>(mPool.get());
        
        mBuffers.resize(info.inflightNum);
        for (uint32_t i = 0; i < info.inflightNum; ++i)
        {
            mBuffers[i] = mMetalPool->Allocate();
        }

        if (info.parallelNum > 1)
        {
            mThreadPool = std::make_unique<ThreadPool>(info.parallelNum, [](uint32_t) {
                return new MetalThreadContext();
            });

            mParallelPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
            mMetalParallelPool = static_cast<MetalCommandPool*>(mParallelPool.get());

            const uint32_t parallelCount = info.parallelNum * info.inflightNum;
            mParallelBuffers.resize(parallelCount);
            for (uint32_t i = 0; i < parallelCount; ++i)
            {
                mParallelBuffers[i] = mMetalParallelPool->Allocate();
            }
        }
    }

    MetalDeviceFrameContext::~MetalDeviceFrameContext() noexcept
    {
        mThreadPool = nullptr;
        mParallelPool = nullptr;
        mMetalParallelPool = nullptr;
        mPool = nullptr;
        mMetalPool = nullptr;
    }
} // sky::aurora
