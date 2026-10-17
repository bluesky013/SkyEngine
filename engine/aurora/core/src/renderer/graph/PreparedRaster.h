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
        RenderResourceKey      resource;
        ShaderPtr              shader;
        BlendState             blend;
        RasterDepthState       depth;
        std::vector<std::byte> constants;
    };
    // Column-major matrices: set 0/b0 View, set 1/b0 material bytes, set 2/b0 Object.
    struct PreparedViewConstants {
        Matrix4 viewProjection;
        Vector4 cameraPosition;
    };
    struct PreparedObjectConstants {
        Matrix4 world;
    };
    struct PreparedRasterView {
        std::vector<RasterDraw>                                  draws;
        std::vector<std::shared_ptr<const ResourceVersionLease>> pins;
    };
    struct PreparedGraphBuffer {
        RenderResourceKey resource;
        GraphBufferHandle handle;
    };

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
                                                                       std::span<const PreparedGraphBuffer> inGraph = {});
} // namespace sky::aurora
