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
        MetalCommandBuffer(MetalDevice &device, void *queue);
        ~MetalCommandBuffer() override;

        void Begin() override;
        void End() override;
        void PipelineBarrier(const BarrierInfo &info) override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override;

        void *GetNativeHandle() const { return cmdBuffer; }    // id<MTLCommandBuffer>

        // Backend-only: encoders call these on construction/destruction so the
        // command buffer can route in-encoder barriers and flush pending ones.
        enum class ActiveEncoderKind { None, Render, Compute, Blit };
        void NotifyEncoderBegin(ActiveEncoderKind kind, void *encoder);
        void NotifyEncoderEnd();

    private:
        MetalDevice          &device;
        void                 *queue          = nullptr;  // id<MTLCommandQueue>, not owned
        void                 *cmdBuffer      = nullptr;  // id<MTLCommandBuffer>
        void                 *activeEncoder  = nullptr;  // typed Obj-C encoder (render/compute/blit)
        ActiveEncoderKind     activeKind     = ActiveEncoderKind::None;
        std::vector<BarrierInfo> pendingBarriers;        // queued until next encoder begins
    };

    class MetalCommandPool : public CommandPool {
    public:
        MetalCommandPool(MetalDevice &device, void *queue);
        ~MetalCommandPool() override;

        bool Init() override;
        void Reset() override;
        CommandBuffer *Allocate() override;

    private:
        MetalDevice &device;
        void        *queue = nullptr;
        std::vector<MetalCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora
