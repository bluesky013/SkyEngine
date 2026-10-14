#pragma once

#include <aurora/renderer/RenderResourceTypes.h>
#include <aurora/renderer/RendererTypes.h>
#include <variant>
#include <vector>

namespace sky::aurora {

    struct RenderEntityDesc {
        Matrix4               world = RenderIdentityMatrix();
        RenderResourceBinding mesh;
        RenderResourceBinding material;
        uint64_t              visibilityMask = ~uint64_t{0};
    };

    struct CreateScene {
        SceneId   id;
        SceneDesc desc;
    };
    struct DestroyScene {
        SceneId id;
    };
    struct CreateRenderEntity {
        RenderEntityRef  id;
        RenderEntityDesc desc;
    };
    struct UpdateRenderEntity {
        RenderEntityRef  id;
        RenderEntityDesc desc;
        uint64_t         revision = 0;
    };
    struct DestroyRenderEntity {
        RenderEntityRef id;
    };
    struct CreateView {
        ViewId   id;
        ViewDesc desc;
    };
    struct UpdateView {
        ViewId   id;
        ViewDesc desc;
        uint64_t revision = 0;
    };
    struct DestroyView {
        ViewId id;
    };
    struct CreateViewport {
        ViewportId   id;
        ViewportDesc desc;
    };
    struct ResizeViewport {
        ViewportId id;
        Extent2D   extent;
        uint64_t   revision = 0;
    };
    struct DestroyViewport {
        ViewportId id;
    };
    struct ResourceCpuReady {
        RenderResourcePayload payload;
    };

    // Encoded resources use logical IDs/revisions, never RHI pointers.
    struct FrameExtensionPayload {
        Name                 feature;
        std::vector<uint8_t> bytes;
    };

    using RenderCommand = std::variant<CreateScene,
                                       DestroyScene,
                                       CreateRenderEntity,
                                       UpdateRenderEntity,
                                       DestroyRenderEntity,
                                       CreateView,
                                       UpdateView,
                                       DestroyView,
                                       CreateViewport,
                                       ResizeViewport,
                                       DestroyViewport,
                                       ResourceCpuReady>;

    struct FramePacket {
        // Nonzero and strictly increasing per renderer; accepted packets/commands apply FIFO.
        // Retry the same packet on BUSY; async failures report this sequence.
        uint64_t                           sequence     = 0;
        double                             timeSeconds  = 0;
        float                              deltaSeconds = 0;
        std::vector<RenderCommand>         commands;
        std::vector<FrameExtensionPayload> extensions;

        FramePacket()                               = default;
        FramePacket(FramePacket &&)                 = default;
        FramePacket &operator=(FramePacket &&)      = default;
        FramePacket(const FramePacket &)            = delete;
        FramePacket &operator=(const FramePacket &) = delete;
    };

    using RendererEvent = std::variant<PacketApplied,
                                       SceneDestroyed,
                                       EntityDestroyed,
                                       ViewDestroyed,
                                       ViewportDestroyed,
                                       ResourceStateChanged,
                                       RendererStopped,
                                       RendererFailure>;

} // namespace sky::aurora
