#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>
#include <aurora/renderer/RenderCommands.h>
#include <memory>

namespace sky::aurora {

    // Freeze exact bindings/fallbacks before prepare and pin their versions in provider caches.
    // Finalize must not replace the mesh used for bounds/culling.
    struct PreparedDrawPacket {
        RenderEntityRef       entity;
        Matrix4               world = RenderIdentityMatrix();
        RenderResourceBinding mesh;
        RenderResourceBinding material;
        uint32_t              subMesh = 0;
    };

    struct PreparedViewData {
        ViewId                          id;
        ViewDesc                        view;
        uint64_t                        sceneRevision = 0;
        std::vector<PreparedDrawPacket> draws;
    };

    struct PreparedFrame {
        uint64_t                           packetSequence = 0;
        std::vector<PreparedViewData>      views;
        std::vector<FrameExtensionPayload> extensions;
    };

    struct FrameOutputInfo {
        ViewportId  viewport;
        uint64_t    surfaceRevision = 0;
        Extent2D    extent{1, 1}; // actual acquired/output extent
        PixelFormat format = PixelFormat::RGBA8_UNORM;
    };

    // Provider build-context storage owns native outputs/uploads/pins for this lease.
    // Freeze transfers recording/GPU retention to the backend plan.
    struct FrameBuildInputs {
        std::shared_ptr<const PreparedFrame> prepared;
        DeviceFrameLease                     lease;
        std::vector<FrameOutputInfo>         outputs;
    };

} // namespace sky::aurora
