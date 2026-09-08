//
// Aurora RDG internal graph structures (hand-rolled, no boost).
//

#pragma once

#include <core/name/Name.h>
#include <core/memory/TransientAllocator.h>
#include <aurora/rhi/Core.h>

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
    struct RasterPassData {
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

        std::function<void(GraphicsEncoder &, RDGContext &)> executeFn;

        explicit RasterPassData(TransientAllocator &alloc)
            : colors(TransientStdAllocator<ColorAttachmentRef>{alloc})
        {
        }
    };

    struct ComputePassData {
        std::function<void(ComputeEncoder &, RDGContext &)> executeFn;
    };

    struct CopyPassData {
        std::function<void(BlitEncoder &, RDGContext &)> executeFn;
    };

    // ---- pass tags (variant dispatch) ----
    struct RasterPassTag {};
    struct ComputePassTag {};
    struct CopyPassTag {};
    using PassTag = std::variant<RasterPassTag, ComputePassTag, CopyPassTag>;

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
