//
// Aurora RDG internal graph structures (hand-rolled, no boost).
//

#pragma once

#include <core/name/Name.h>
#include <core/memory/TransientAllocator.h>
#include <aurora/rhi/Core.h>

#include <aurora/rdg/CompiledGraph.h>
#include <aurora/rdg/RDGHandles.h>
#include <aurora/rdg/RDGTypes.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Buffer.h>

#include <functional>
#include <variant>
#include <vector>

namespace sky::aurora {

    class GraphicsEncoder;
    class ComputeEncoder;
    class BlitEncoder;
    class RDGContext;
    class ResourceGroup;
    class GraphicsPipeline;
    class ComputePipeline;

    // ---- lifetime ----
    struct LifeTime {
        uint32_t firstUsePass = INVALID_INDEX;
        uint32_t lastUsePass  = 0;
    };

    // ---- access chain record (per resource, ordered by pass) ----
    // v1 tracks whole-resource access; subRange (mip/layer/aspect) is reserved for
    // v2 TrackSubresource precision.
    struct AccessRecord {
        uint32_t    pass = INVALID_INDEX;
        AccessFlags access;
    };

    // ---- resource payloads ----
    // transient payloads carry only the descriptor; the resolved backing image /
    // buffer is stored in RenderGraph::mResolvedImages/mResolvedBuffers (indexed by
    // resource index) and owned by the transient pool.
    struct GraphImage {
        RDGTextureDesc desc;
    };

    struct GraphImportImage {
        ImagePtr      image        = nullptr;
        AccessFlags   importAccess = AccessFlagBit::NONE;
        ImageLayout   importLayout = ImageLayout::UNDEFINED;
    };

    struct GraphBuffer {
        RDGBufferDesc desc;
    };

    struct GraphImportBuffer {
        BufferPtr     buffer       = nullptr;
        AccessFlags   importAccess = AccessFlagBit::NONE;
    };

    // ---- resource tags (variant dispatch) ----
    struct TransientImageTag {};
    struct ImportImageTag {};
    struct TransientBufferTag {};
    struct ImportBufferTag {};
    using ResourceTag = std::variant<TransientImageTag, ImportImageTag, TransientBufferTag, ImportBufferTag>;

    struct ResourceNode {
        Name        name;
        ResourceTag tag;
        uint32_t    payloadIndex  = INVALID_INDEX;
        LifeTime    lifeTime;
        uint32_t    lastWriterPass = INVALID_INDEX;
        TransientVector<AccessRecord> accesses;

        explicit ResourceNode(TransientAllocator &alloc)
            : accesses(TransientStdAllocator<AccessRecord>{alloc})
        {
        }
    };

    // ---- pass payloads ----
    struct SceneRasterPassData {
        struct ColorAttachmentRef {
            uint32_t   slot          = 0;
            uint32_t   resourceIndex = INVALID_INDEX;
            LoadOp     loadOp        = LoadOp::DONT_CARE;
            StoreOp    storeOp       = StoreOp::STORE;
            ClearValue clearValue{0.f, 0.f, 0.f, 0.f};
        };

        TransientVector<ColorAttachmentRef> colors;
        Extent2D    renderArea{1, 1};

        uint32_t   depthStencilResource = INVALID_INDEX;
        LoadOp     depthLoadOp          = LoadOp::DONT_CARE;
        StoreOp    depthStoreOp         = StoreOp::STORE;
        LoadOp     stencilLoadOp        = LoadOp::DONT_CARE;
        StoreOp    stencilStoreOp       = StoreOp::DONT_CARE;
        ClearValue depthStencilClear{0.f, 0};

        TransientVector<SceneRasterQueue> queues; // declaration order

        ResourceGroup *passResourceGroup = nullptr; // set 1 (pass-level, queue RG overrides per queue)

        explicit SceneRasterPassData(TransientAllocator &alloc)
            : colors(TransientStdAllocator<ColorAttachmentRef>{alloc})
            , queues(TransientStdAllocator<SceneRasterQueue>{alloc})
        {
        }
    };

    struct FullScreenPassData {
        GraphicsPipeline *pso = nullptr;
        ResourceGroup    *passResourceGroup = nullptr;

        TransientVector<SceneRasterPassData::ColorAttachmentRef> colors;
        Extent2D    renderArea{1, 1};

        uint32_t   depthStencilResource = INVALID_INDEX;
        LoadOp     depthLoadOp          = LoadOp::DONT_CARE;
        StoreOp    depthStoreOp         = StoreOp::STORE;
        LoadOp     stencilLoadOp        = LoadOp::DONT_CARE;
        StoreOp    stencilStoreOp       = StoreOp::DONT_CARE;
        ClearValue depthStencilClear{0.f, 0};

        explicit FullScreenPassData(TransientAllocator &alloc)
            : colors(TransientStdAllocator<SceneRasterPassData::ColorAttachmentRef>{alloc})
        {
        }
    };

    struct ComputePassData {
        ComputePipeline *pso = nullptr;
        ResourceGroup   *passResourceGroup = nullptr;
        uint32_t        groupX = 1, groupY = 1, groupZ = 1;

        std::function<void(ComputeEncoder &, RDGContext &)> executeFn;
    };

    struct CopyBlitPassData {
        CopyBlitPayload::Kind kind = CopyBlitPayload::Kind::BUFFER;
        uint32_t srcResourceIndex = INVALID_INDEX;
        uint32_t dstResourceIndex = INVALID_INDEX;
        uint64_t size      = 0;
        uint64_t srcOffset = 0;
        uint64_t dstOffset = 0;

        std::function<void(BlitEncoder &, RDGContext &)> executeFn;
    };

    struct PresentPassData {
        uint32_t imageResourceIndex = INVALID_INDEX;
    };

    struct CustomPassData {
        std::function<void(RDGContext &, CommandBuffer &)> fn;
    };

    // ---- pass tags (variant dispatch) ----
    struct SceneRasterPassTag {};
    struct FullScreenPassTag {};
    struct ComputePassTag {};
    struct CopyBlitPassTag {};
    struct PresentPassTag {};
    struct CustomPassTag {};
    using PassTag = std::variant<SceneRasterPassTag, FullScreenPassTag, ComputePassTag,
                                 CopyBlitPassTag, PresentPassTag, CustomPassTag>;

    struct PassNode {
        Name         name;
        PassTag      tag;
        uint32_t     payloadIndex = INVALID_INDEX;
        TransientVector<uint32_t> readResources;
        TransientVector<uint32_t> writeResources;
        TransientVector<uint32_t> dependsOn;   // pass indices this pass depends on
        uint32_t     inDegree = 0;         // for Kahn topological sort
        bool         live     = false;
        TransientVector<BarrierInfo> frontBarriers;   // barriers emitted before this pass

        explicit PassNode(TransientAllocator &alloc)
            : readResources(TransientStdAllocator<uint32_t>{alloc})
            , writeResources(TransientStdAllocator<uint32_t>{alloc})
            , dependsOn(TransientStdAllocator<uint32_t>{alloc})
            , frontBarriers(TransientStdAllocator<BarrierInfo>{alloc})
        {
        }
    };

} // namespace sky::aurora
