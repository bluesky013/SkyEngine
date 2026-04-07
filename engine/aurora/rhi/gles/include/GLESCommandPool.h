//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/CommandBuffer.h>
#include <vector>
#include <memory>

namespace sky::aurora {

    class GLESDevice;

    class GLESCommandBuffer : public CommandBuffer {
    public:
        explicit GLESCommandBuffer(GLESDevice &device);
        ~GLESCommandBuffer() override = default;

        void Begin() override;
        void End() override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override;

    private:
        GLESDevice &device;
    };

    class GLESCommandPool : public CommandPool {
    public:
        explicit GLESCommandPool(GLESDevice &device);
        ~GLESCommandPool() override;

        bool Init() override;
        void Reset() override;
        CommandBuffer *Allocate() override;

    private:
        GLESDevice &device;
        std::vector<GLESCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora
