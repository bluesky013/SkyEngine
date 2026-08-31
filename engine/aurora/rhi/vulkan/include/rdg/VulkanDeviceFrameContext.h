//
// Created on 2026/08/31.
//

#pragma once

#include <VulkanCommandPool.h>
#include <aurora/rdg/RenderDeviceExclusive.h>

namespace sky::aurora {
    class VulkanDevice;

    class VulkanDeviceFrameContext : public DeviceFrameContext {
    public:
        explicit VulkanDeviceFrameContext(VulkanDevice *device, const DeviceFrameContextInitInfo &info);
        ~VulkanDeviceFrameContext() noexcept override;

    private:
        VulkanDevice                *mDevice;
        std::unique_ptr<CommandPool> mPool;
        VulkanCommandPool           *mVulkanPool = nullptr;
        std::vector<CommandBuffer *> mBuffers;

        std::vector<std::vector<CommandBuffer *>> mParallelBuffers;
    };

} // namespace sky::aurora
