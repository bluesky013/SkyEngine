//
// Aurora RDG compiled output (flat, live-only, executor-ready).
//

#pragma once

#include <core/memory/TransientAllocator.h>
#include <core/name/Name.h>

#include <aurora/rdg/RDGHandles.h>
#include <aurora/rdg/RDGTypes.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Buffer.h>

#include <functional>
#include <variant>

namespace sky::aurora {

    class GraphicsEncoder;
    class ComputeEncoder;
    class BlitEncoder;
    class RDGContext;
    class ResourceGroup;
    class CommandBuffer;
    class GraphicsPipeline;
    class ComputePipeline;

    // ---- compiled pass types ----
    enum class CompiledPassType : uint8_t {
        SCENE_RASTER = 0,
        FULLSCREEN,
        COMPUTE,
        COPYBLIT,
        PRESENT,
        CUSTOM,
    };

    // ---- attachments ----
    struct CompiledColorAttachment {
        uint32_t   slot          = 0;
        Image     *image         = nullptr; // resolved backing image
        LoadOp     loadOp        = LoadOp::DONT_CARE;
        StoreOp    storeOp       = StoreOp::STORE;
        ClearValue clearValue{0.f, 0.f, 0.f, 0.f};
    };

    struct CompiledDepthStencilAttachment {
        Image     *image           = nullptr;
        LoadOp     depthLoadOp     = LoadOp::DONT_CARE;
        StoreOp    depthStoreOp    = StoreOp::STORE;
        LoadOp     stencilLoadOp   = LoadOp::DONT_CARE;
        StoreOp    stencilStoreOp  = StoreOp::DONT_CARE;
        ClearValue clearValue{0.f, 0};
    };

    // ---- DrawItem (per-item PSO + Batch ResourceGroup) ----
    struct DrawItem {
        GraphicsPipeline *pso = nullptr;                // per-item PSO reference
        ResourceGroup    *batchResourceGroup = nullptr; // set 2

        Buffer *vb = nullptr;
        Buffer *ib = nullptr;
        uint32_t vbOffset = 0;
        uint32_t ibOffset = 0;
        CmdDrawIndexed args;
    };

    // ---- payloads ----
    struct SceneRasterPayload {
        ResourceGroup *passResourceGroup = nullptr; // set 1

        TransientVector<CompiledColorAttachment> colors;
        CompiledDepthStencilAttachment depthStencil;
        TransientVector<DrawItem> items; // upper-layer sorted

        explicit SceneRasterPayload(TransientAllocator &alloc)
            : colors(TransientStdAllocator<CompiledColorAttachment>{alloc})
            , items(TransientStdAllocator<DrawItem>{alloc})
        {
        }
    };

    struct FullScreenPayload {
        GraphicsPipeline *pso = nullptr;                // fullscreen triangle pipeline
        ResourceGroup    *passResourceGroup = nullptr;  // set 1

        TransientVector<CompiledColorAttachment> colors;
        CompiledDepthStencilAttachment depthStencil;

        explicit FullScreenPayload(TransientAllocator &alloc)
            : colors(TransientStdAllocator<CompiledColorAttachment>{alloc})
        {
        }
    };

    struct ComputePayload {
        ComputePipeline *pso = nullptr;                 // compute pipeline
        ResourceGroup   *passResourceGroup = nullptr;   // set 1

        uint32_t groupX = 1;
        uint32_t groupY = 1;
        uint32_t groupZ = 1;
    };

    struct CopyBlitPayload {
        enum class Kind : uint8_t {
            BUFFER = 0,
            IMAGE,
            BUFFER_TO_IMAGE,
            IMAGE_TO_BUFFER,
        };

        Kind kind = Kind::BUFFER;

        Buffer *srcBuffer = nullptr;
        Buffer *dstBuffer = nullptr;
        Image  *srcImage  = nullptr;
        Image  *dstImage  = nullptr;

        uint64_t size      = 0;
        uint64_t srcOffset = 0;
        uint64_t dstOffset = 0;
    };

    struct PresentPayload {
        Image *image = nullptr; // swapchain image
    };

    struct CustomPayload {
        // escape hatch for device extensions (MetalFX / DLSS / etc.)
        std::function<void(RDGContext &, CommandBuffer &)> fn;
    };

    // ---- compiled pass ----
    struct CompiledPass {
        CompiledPassType type      = CompiledPassType::SCENE_RASTER;
        uint32_t         passIndex = INVALID_INDEX; // index into original setup graph
        Name             name;

        // barrier segment: contiguous range in CompiledGraph::barriers
        uint32_t barrierOffset = 0;
        uint32_t barrierCount  = 0;

        // variant payload (replaces std::function fields)
        std::variant<SceneRasterPayload, FullScreenPayload, ComputePayload,
                     CopyBlitPayload, PresentPayload, CustomPayload> payload;

        explicit CompiledPass(TransientAllocator &alloc)
            : payload(SceneRasterPayload{alloc})
        {
        }
    };

    // ---- compiled graph ----
    struct CompiledGraph {
        ResourceGroup *globalResourceGroup = nullptr; // set 0

        TransientVector<CompiledPass> passes;       // live only, topo order
        TransientVector<BarrierInfo>  barriers;     // flat barrier array
        TransientVector<Image *>      resolvedImages;
        TransientVector<Buffer *>     resolvedBuffers;
        TransientVector<uint32_t>     topologicalOrder;

        explicit CompiledGraph(TransientAllocator &alloc)
            : passes(TransientStdAllocator<CompiledPass>{alloc})
            , barriers(TransientStdAllocator<BarrierInfo>{alloc})
            , resolvedImages(TransientStdAllocator<Image *>{alloc})
            , resolvedBuffers(TransientStdAllocator<Buffer *>{alloc})
            , topologicalOrder(TransientStdAllocator<uint32_t>{alloc})
        {
        }
    };

} // namespace sky::aurora
