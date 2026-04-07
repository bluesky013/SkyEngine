//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/CommandBuffer.h>
#include <vector>
#include <memory>

namespace sky::aurora {

    class MetalDevice;

    class MetalCommandBuffer : public CommandBuffer {
    public:
        explicit MetalCommandBuffer(MetalDevice &device);
        ~MetalCommandBuffer() override;

        void Begin() override;
        void End() override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override;

    private:
        MetalDevice &device;
        void *cmdBuffer = nullptr; // id<MTLCommandBuffer>
    };

    class MetalCommandPool : public CommandPool {
    public:
        explicit MetalCommandPool(MetalDevice &device);
        ~MetalCommandPool() override;

        bool Init() override;
        void Reset() override;
        CommandBuffer *Allocate() override;

    private:
        MetalDevice &device;
        std::vector<MetalCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora
