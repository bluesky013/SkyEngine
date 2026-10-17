//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/SubmissionResult.h>
#include <memory>

namespace sky::aurora {

    class Device;

    class GraphicsEncoder;
    class ComputeEncoder;
    class BlitEncoder;

    class CommandBuffer {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

        CommandBuffer()          = default;
        virtual ~CommandBuffer() = default;

        virtual void Begin() = 0;
        virtual void End()   = 0;

        // Emit a pipeline barrier. May be called between or within encoder
        // record blocks; backends route accordingly.
        virtual void PipelineBarrier(const BarrierInfo &info) = 0;

        virtual std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() = 0;
        virtual std::unique_ptr<ComputeEncoder>  CreateComputeEncoder()  = 0;
        virtual std::unique_ptr<BlitEncoder>     CreateBlitEncoder()     = 0;
    };

    class CommandPool {
    public:
        CommandPool()          = default;
        virtual ~CommandPool() = default;

        virtual bool               Init()  = 0;
        virtual void               Reset() = 0;
        virtual GpuOperationResult TryReset() noexcept
        {
            return {{GpuErrorCode::UNSUPPORTED}};
        }
        virtual CommandBuffer *Allocate() = 0;
    };

} // namespace sky::aurora
