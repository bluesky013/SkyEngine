//
// Created by Zach Lee on 2026/8/9.
//

#pragma once

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include <aurora/rdg/RenderDeviceExclusive.h>
#include <MetalCommandPool.h>

namespace sky::aurora {
    class MetalDevice;

    class MetalDeviceFrameContext : public DeviceFrameContext {
    public:
        explicit MetalDeviceFrameContext(MetalDevice* device, const DeviceFrameContextInitInfo& info);
        ~MetalDeviceFrameContext() noexcept override;

    private:
        MetalDevice* mDevice;
        
        uint32_t mInflightCommands;
        
        std::unique_ptr<CommandPool> mPool;
        MetalCommandPool* mMetalPool = nullptr;
        
        std::vector<CommandBuffer*> mBuffers;
    };

} // namespace sky::aurora
