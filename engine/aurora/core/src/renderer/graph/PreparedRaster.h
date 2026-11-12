//
// Created on 2026/10/16.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/renderer/PreparedFrameTypes.h>
#include <map>
#include <span>
#include <tuple>

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
    struct PreparedMaterialTechniques {
        RenderResourceKey                                    resource;
        std::vector<std::pair<Name, PreparedMaterialLayout>> techniques;
    };
    struct PreparedRasterView {
        std::vector<ResolvedRasterDraw>                          draws;
        std::vector<std::shared_ptr<const ResourceVersionLease>> pins;
    };
    struct PreparedGraphBuffer {
        RenderResourceKey resource;
        GraphBufferHandle handle;
    };

    // Built after graph-local uploads resolve; layout borrows end before the frame's catalog moves into the frozen graph.
    struct PreparedRasterIndex {
        using ResourceKey = std::tuple<Uuid, uint64_t, uint64_t, uint64_t>;
        static ResourceKey Key(const RenderResourceKey &key)
        {
            return {key.product, key.usageVariant, key.deviceEpoch, key.revision};
        }
        std::map<ResourceKey, std::shared_ptr<const ResourceVersionLease>>                 pins;
        std::map<ResourceKey, const PreparedMeshLayout *>                                  meshes;
        std::map<ResourceKey, std::map<Name, std::vector<const PreparedMaterialLayout *>>> materials;
        std::map<ResourceKey, GraphBufferHandle>                                           localBuffers;
        bool                                                                               GeometryReady(const PreparedDrawPacket &) const;
    };
    std::variant<FrameError, PreparedRasterIndex> BuildPreparedRasterIndex(const FrozenResourceCatalog &,
                                                                           std::span<const PreparedMeshLayout>,
                                                                           std::span<const PreparedMaterialTechniques>,
                                                                           std::span<const PreparedGraphBuffer> = {});

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
                                                                       SceneId,
                                                                       std::span<const PreparedDrawPacket>,
                                                                       std::span<const std::shared_ptr<const RenderSkinningData>>,
                                                                       const PreparedRasterIndex &,
                                                                       Name                 tag,
                                                                       Rect2D               scissor,
                                                                       const ShaderVariant &pipelineValues = {});
} // namespace sky::aurora
