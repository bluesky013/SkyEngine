//
// Created on 2026/10/16.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/renderer/PreparedFrameTypes.h>
#include <span>

namespace sky::aurora {
    class DeviceFrameContext;
    class FrameGraphBuilder;
    struct FrozenResourceCatalog;
    struct ResourceVersionLease;
    using PreparedMeshStream = RenderMeshStream;
    using PreparedMeshLod    = RenderMeshLod;
    struct PreparedMeshLayout {
        RenderResourceKey                  resource;
        std::vector<PreparedMeshStream>    streams;
        std::vector<RasterVertexAttribute> attributes;
        std::vector<PreparedMeshLod>       lods;
    };
    struct PreparedMaterialLayout {
        RenderResourceKey        resource;
        ShaderPtr                shader;
        ShaderVariant            pipeline; // non-default pipeline-owned values
        bool                     skinned          = false;
        bool                     pipelineAgnostic = false; // legacy single-program material
        uint16_t                 vertexMask       = 0;
        BlendState               blend;
        RasterDepthState         depth;
        RenderMaterialParameters parameters;
    };
    struct PreparedRasterView {
        std::vector<ResolvedRasterDraw>                          draws;
        std::vector<std::shared_ptr<const ResourceVersionLease>> pins;
    };
    struct PreparedGraphBuffer {
        RenderResourceKey resource;
        GraphBufferHandle handle;
    };

    std::variant<FrameError, std::vector<RasterVertexAttribute>>
    ResolveVertexInputs(std::span<const RasterVertexAttribute>, const ShaderReflection &);

    // Exact P|V selection for one draw. Legacy single-program materials are
    // pipeline-agnostic but still vertex-specific.
    const PreparedMaterialLayout *FindMaterialVariant(std::span<const PreparedMaterialLayout>,
                                                      const RenderResourceKey &,
                                                      bool                 skinned,
                                                      const ShaderVariant &pipelineValues,
                                                      uint16_t             vertexMask = 0);

    // Coordinator-only; on failure discard this graph and its unsubmitted lease before rebuilding.
    std::variant<FrameError, PreparedRasterView> ResolvePreparedRaster(Device &,
                                                                       DeviceFrameContext &,
                                                                       const DeviceFrameLease &,
                                                                       FrameGraphBuilder &,
                                                                       const PreparedViewData &,
                                                                       const FrozenResourceCatalog &,
                                                                       std::span<const PreparedMeshLayout>,
                                                                       std::span<const PreparedMaterialLayout>,
                                                                       Rect2D                               scissor,
                                                                       std::span<const PreparedGraphBuffer> inGraph        = {},
                                                                       PreparedDrawList                     list           = PreparedDrawList::VISIBLE,
                                                                       const ShaderVariant                 &pipelineValues = {});
} // namespace sky::aurora
