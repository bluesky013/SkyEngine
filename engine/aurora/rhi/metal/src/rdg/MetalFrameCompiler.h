#pragma once

#include <MetalBuffer.h>
#include <MetalEncoder.h>
#include <MetalImage.h>
#include <aurora/rhi/PipelineState.h>
#include <aurora/rhi/ResourceGroup.h>
#include <common/FrameContext.h>
#include <common/FrameTransientPool.h>

namespace sky::aurora {
    class MetalDevice;
    class MetalCommandBuffer;

    struct MetalCopyBufferStep {
        MetalBuffer *source            = nullptr;
        MetalBuffer *destination       = nullptr;
        uint64_t     size              = 0;
        uint64_t     sourceOffset      = 0;
        uint64_t     destinationOffset = 0;
    };
    struct MetalCopyImageStep {
        MetalBuffer    *buffer = nullptr;
        MetalImage     *image  = nullptr;
        BufferImageCopy region;
        bool            upload = true;
    };
    struct MetalSampledDepthCopyStep {
        MetalImage *source      = nullptr;
        MetalImage *destination = nullptr;
        uint32_t    baseMip = 0, mipCount = 1, baseLayer = 0, layerCount = 1;
        Extent2D    extent;
    };
    struct MetalSet {
        uint32_t                  set = 0;
        CounterPtr<ResourceGroup> group;
        std::vector<uint32_t>     dynamicOffsets;
    };
    struct MetalDraw {
        CounterPtr<GraphicsPipeline> pipeline;
        std::vector<MetalSet>        sets;
        std::vector<BufferView>      vertexBuffers;
        MetalBuffer                 *indexBuffer = nullptr;
        uint64_t                     indexOffset = 0;
        IndexType                    indexType   = IndexType::U32;
        CmdDrawLinear                command;
        CmdDrawIndexed               indexed;
        Rect2D                       scissor;
        bool                         isIndexed         = false;
        MetalBuffer                 *indirectBuffer    = nullptr;
        uint64_t                     indirectOffset    = 0;
        uint32_t                     indirectDrawCount = 0;
        uint32_t                     indirectStride    = 0;
    };
    struct MetalRasterStep {
        MetalImage            *color      = nullptr;
        MetalImage            *depth      = nullptr;
        LoadOp                 load       = LoadOp::CLEAR;
        StoreOp                store      = StoreOp::STORE;
        LoadOp                 depthLoad  = LoadOp::DONT_CARE;
        StoreOp                depthStore = StoreOp::DONT_CARE;
        ClearValue             clear;
        ClearValue             depthClear;
        Extent2D               extent;
        MetalRenderingResolves resolves;
        std::vector<MetalDraw> draws;
    };
    struct MetalComputeStep {
        CounterPtr<ComputePipeline> pipeline;
        std::vector<MetalSet>       sets;
        uint32_t                    groupsX = 1, groupsY = 1, groupsZ = 1;
        MetalBuffer                *indirectBuffer = nullptr;
        uint64_t                    indirectOffset = 0;
    };
    struct MetalCustomStep {
        std::shared_ptr<const FrameCustomPassEncoder> encoder;
        std::vector<Image *>                          images;
        std::vector<Buffer *>                         buffers;
    };
    using MetalStep =
        std::variant<MetalCopyBufferStep, MetalCopyImageStep, MetalSampledDepthCopyStep, MetalRasterStep, MetalComputeStep, MetalCustomStep>;

    class MetalFrameEncoder final {
    public:
        std::optional<FrameError> Encode(MetalDevice &, MetalCommandBuffer &) const;

        std::vector<MetalStep>              steps;
        std::vector<CounterPtr<MetalImage>> views;
    };

    class MetalCompiledFrame final : public BackendCompiledFrame {
    public:
        CompiledFrameIdentity GetIdentity() const noexcept override
        {
            return reservation.identity;
        }

        FrameResourceBindings                   resources;
        FramePlanLease                          reservation;
        std::shared_ptr<const FrozenFrameGraph> graph;
        std::vector<uint32_t>                   livePasses;
        std::unique_ptr<MetalFrameEncoder>      encoder;
    };
    using MetalCompileResult = std::variant<FrameError, FrameNoWork, std::unique_ptr<MetalCompiledFrame>>;

    class MetalFrameCompiler final {
    public:
        explicit MetalFrameCompiler(MetalDevice &device) : device(device)
        {
        }
        MetalCompileResult Compile(std::shared_ptr<const FrozenFrameGraph>, uint64_t epoch);

    private:
        MetalDevice &device;
    };
} // namespace sky::aurora
