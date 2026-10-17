#pragma once

#include <algorithm>
#include <aurora/renderer/RenderResourceTypes.h>
#include <aurora/renderer/RendererTypes.h>
#include <cmath>
#include <memory>
#include <variant>
#include <vector>

namespace sky::aurora {
    class IFrameGraphFeature;

    struct RenderPointLight {
        Vector3 color{1, 1, 1};
        float   intensity = 0;
        float   range     = 0;
    };
    struct RenderDrawProperties {
        uint64_t         tags = 1;
        RenderPointLight light;
    };

    struct RenderEntityDesc {
        Matrix4               world = RenderIdentityMatrix();
        RenderResourceBinding mesh;
        RenderResourceBinding material;
        uint64_t              visibilityMask = ~uint64_t{0};
        RenderDrawProperties  properties;
    };

    inline bool IsValidRenderEntityDesc(const RenderEntityDesc &desc)
    {
        const auto &m = desc.world;
        const auto &l = desc.properties.light;
        return std::all_of(std::begin(m.v), std::end(m.v), [](float v) { return std::isfinite(v); }) && m.v[3] == 0 && m.v[7] == 0 && m.v[11] == 0 &&
               m.v[15] == 1 && std::isfinite(l.color.x) && std::isfinite(l.color.y) && std::isfinite(l.color.z) && l.color.x >= 0 && l.color.y >= 0 &&
               l.color.z >= 0 && std::isfinite(l.intensity) && l.intensity >= 0 && std::isfinite(l.range) && l.range >= 0;
    }

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
    // All Update/Resize revisions increase per handle generation; zero is reserved for the creation baseline.
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
        Name                           feature;
        std::vector<uint8_t>           bytes;
        std::vector<ViewportId>        outputs;
        std::vector<RenderResourceKey> resources;
    };

    struct RegisterRenderFeature {
        RenderFeatureId id;
        Name            name;
        // Retain the defining module until the feature and all frame borrowers are released.
        std::shared_ptr<const void>         moduleLease;
        std::shared_ptr<IFrameGraphFeature> feature;
    };
    struct SetRenderFeatureEnabled {
        RenderFeatureId id;
        bool            enabled = true;
    };
    struct RetireRenderFeature {
        RenderFeatureId id;
    };
    using RenderFeatureCommand = std::variant<RegisterRenderFeature, SetRenderFeatureEnabled, RetireRenderFeature>;

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
        std::vector<RenderFeatureCommand>  featureCommands;
        // Drop standalone ownership; queued frames and uploads retain their exact versions.
        std::vector<RenderResourceKey> releasedResources;
        std::vector<ViewportId>        readbacks; // Owned offscreen pixels are delivered after this frame's GPU completion.

        FramePacket()                               = default;
        FramePacket(FramePacket &&)                 = default;
        FramePacket &operator=(FramePacket &&)      = default;
        FramePacket(const FramePacket &)            = delete;
        FramePacket &operator=(const FramePacket &) = delete;
    };

    struct ViewportReadback {
        uint64_t             sequence = 0;
        ViewportId           viewport;
        Extent2D             extent;
        PixelFormat          format = PixelFormat::UNDEFINED;
        std::vector<uint8_t> bytes;
    };

    using RendererEvent = std::variant<PacketApplied,
                                       FrameCompleted,
                                       ViewportReadback,
                                       SceneDestroyed,
                                       ScenePrepareFailed,
                                       EntityDestroyed,
                                       ViewDestroyed,
                                       ViewportDestroyed,
                                       ViewportFailed,
                                       ResourceStateChanged,
                                       RenderFeatureRetired,
                                       RendererStopped,
                                       RendererFailure>;

} // namespace sky::aurora
