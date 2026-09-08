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

namespace sky::aurora {

    class GraphicsEncoder;
    class ComputeEncoder;
    class BlitEncoder;
    class RDGContext;

    // ---- compiled pass types ----
    enum class CompiledPassType : uint8_t {
        RASTER = 0,
        COMPUTE,
        COPY,
    };

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

    struct CompiledPass {
        CompiledPassType type      = CompiledPassType::RASTER;
        uint32_t         passIndex = INVALID_INDEX; // index into original setup graph
        Name             name;

        // barrier segment: contiguous range in CompiledGraph::barriers
        uint32_t barrierOffset = 0;
        uint32_t barrierCount  = 0;

        // raster attachments (resolved)
        TransientVector<CompiledColorAttachment> colors;
        CompiledDepthStencilAttachment depthStencil;

        // execute callback (from setup graph, copied)
        std::function<void(GraphicsEncoder &, RDGContext &)> rasterExecuteFn;
        std::function<void(ComputeEncoder &, RDGContext &)>  computeExecuteFn;
        std::function<void(BlitEncoder &, RDGContext &)>     copyExecuteFn;

        explicit CompiledPass(TransientAllocator &alloc)
            : colors(TransientStdAllocator<CompiledColorAttachment>{alloc})
        {
        }
    };

    // ---- compiled graph ----
    struct CompiledGraph {
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
