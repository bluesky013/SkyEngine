//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <memory>

namespace sky::aurora {

    class GraphicsEncoder;
    class ComputeEncoder;
    class BlitEncoder;

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        virtual ~CommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() = 0;
        virtual std::unique_ptr<ComputeEncoder> CreateComputeEncoder() = 0;
        virtual std::unique_ptr<BlitEncoder> CreateBlitEncoder() = 0;
    };

    class CommandPool {
    public:
        CommandPool() = default;
        virtual ~CommandPool() = default;

        virtual bool Init() = 0;
        virtual void Reset() = 0;
        virtual CommandBuffer *Allocate() = 0;
    };

} // namespace sky::aurora
