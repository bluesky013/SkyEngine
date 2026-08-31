//
// Created on 2026/08/31.
//

#include <VulkanDevice.h>
#include <rdg/VulkanDeviceFrameContext.h>

namespace sky::aurora {

    VulkanDeviceFrameContext::VulkanDeviceFrameContext(VulkanDevice *device, const DeviceFrameContextInitInfo &info) : mDevice(device)
    {
        mInflightNum = info.inflightNum;
        mParallelNum = info.parallelNum;

        mPool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        mVulkanPool = static_cast<VulkanCommandPool *>(mPool.get());

        mBuffers.resize(info.inflightNum);
        for (uint32_t i = 0; i < info.inflightNum; ++i) {
            mBuffers[i] = mVulkanPool->Allocate();
        }

        if (info.parallelNum > 1) {
            mThreadPool = std::make_unique<ThreadPool>(
                info.parallelNum, [device](uint32_t) { return new VulkanContext(*device, QueueType::GRAPHICS, VK_COMMAND_BUFFER_LEVEL_SECONDARY); });

            mParallelBuffers.resize(info.parallelNum);
            for (uint32_t w = 0; w < info.parallelNum; ++w) {
                auto *ctx = static_cast<VulkanContext *>(mThreadPool->GetContext(w));
                mParallelBuffers[w].resize(info.inflightNum);
                for (uint32_t f = 0; f < info.inflightNum; ++f) {
                    mParallelBuffers[w][f] = ctx->pool->Allocate();
                }
            }
        }
    }

    VulkanDeviceFrameContext::~VulkanDeviceFrameContext() noexcept
    {
        mThreadPool = nullptr;
        mPool       = nullptr;
        mVulkanPool = nullptr;
    }

} // namespace sky::aurora
