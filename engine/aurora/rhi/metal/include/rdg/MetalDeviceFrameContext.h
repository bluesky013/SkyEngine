//
// Created by Zach Lee on 2026/8/9.
//

#pragma once

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <MetalCommandPool.h>
#include <aurora/rdg/RenderDeviceExclusive.h>

namespace sky::aurora {
    class MetalDevice;

    class MetalDeviceFrameContext : public DeviceFrameContext {
    public:
        explicit MetalDeviceFrameContext(MetalDevice *device, const DeviceFrameContextInitInfo &info);
        ~MetalDeviceFrameContext() noexcept override;

    private:
        MetalDevice *mDevice;

        uint32_t mInflightCommands;

        std::unique_ptr<CommandPool> mPool;
        MetalCommandPool            *mMetalPool = nullptr;

        std::vector<CommandBuffer *> mBuffers;

        std::unique_ptr<CommandPool> mParallelPool;
        MetalCommandPool            *mMetalParallelPool = nullptr;

        std::vector<CommandBuffer *> mParallelBuffers;
    };

} // namespace sky::aurora
