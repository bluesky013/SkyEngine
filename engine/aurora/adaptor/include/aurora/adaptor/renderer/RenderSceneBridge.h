#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <memory>
#include <span>

namespace sky {
    class World;
}

namespace sky::aurora {
    enum class SceneSurfaceCoverage { UNKNOWN, OPAQUE, ALPHA_TEST, BLENDED };
    enum class SceneSurfaceCull { NONE, FRONT, BACK };
    struct SceneSurfaceFacts {
        uint64_t                 revision           = 0;
        bool                     positionsPreserved = false, standardDepth = false, counterClockwise = true;
        SceneSurfaceCoverage     coverage                                    = SceneSurfaceCoverage::UNKNOWN;
        SceneSurfaceCull         cull                                        = SceneSurfaceCull::NONE;
        bool                     operator==(const SceneSurfaceFacts &) const = default;
        static SceneSurfaceFacts StaticOpaque(uint64_t revision = 1)
        {
            return {.revision = revision, .positionsPreserved = true, .standardDepth = true, .coverage = SceneSurfaceCoverage::OPAQUE};
        }
        bool SupportsGeometry() const
        {
            return revision && positionsPreserved && standardDepth && coverage == SceneSurfaceCoverage::OPAQUE;
        }
    };
    struct SceneResourcePart {
        uint32_t              part = 0;
        RenderResourceBinding mesh, material;
        SceneSurfaceFacts     surface;
    };
    struct SceneMeshResources {
        std::vector<SceneResourcePart>     parts;
        std::vector<RenderResourcePayload> payloads;
    };
    inline bool SupportsSceneHighlight(const SceneMeshResources &resources, const SceneResourcePart &part)
    {
        if (!part.surface.SupportsGeometry())
            return false;
        const auto payload = std::ranges::find_if(resources.payloads, [&](const auto &value) { return value.resource == part.mesh.resource; });
        if (payload == resources.payloads.end() || !payload->mesh || payload->mesh->lods.empty())
            return false;
        const auto &mesh     = *payload->mesh;
        const auto  position = std::ranges::find_if(mesh.attributes, [](const auto &value) {
            return value.semantic == "POSITION" && value.semanticIndex == 0 && value.format == Format::F_RGB32;
        });
        if (position == mesh.attributes.end() || position->binding >= mesh.streams.size() ||
            mesh.streams[position->binding].rate != VertexInputRate::PER_VERTEX)
            return false;
        const auto &lod = mesh.lods.front();
        if (!lod.sections.empty() && part.part >= lod.sections.size())
            return false;
        const auto &draw = lod.sections.empty() ? lod.command : lod.sections[part.part];
        return draw.instanceCount == 1 && draw.firstInstance == 0;
    }
    using SceneResourceResult = std::variant<std::monostate, RendererError, std::shared_ptr<const SceneMeshResources>>;
    using SceneImageResult    = std::variant<std::monostate, RendererError, std::shared_ptr<const RenderResourcePayload>>;
    enum class SceneGeometryState { READY, PENDING, UNSUPPORTED, ERROR };
    struct SceneGeometryInstance {
        Uuid                                      actor, mesh, material;
        Matrix4                                   world;
        uint64_t                                  visibilityMask = ~uint64_t{0};
        SceneGeometryState                        state          = SceneGeometryState::PENDING;
        std::optional<AABB>                       bounds;
        std::shared_ptr<const SceneMeshResources> resources;
        std::string                               message;
        Uuid                                      component;
        uint64_t generation = 1, transformRevision = 1, geometryRevision = 1, capabilityRevision = 1, readinessRevision = 1;
        bool     selectable = true, occluder = true;
    };
    struct SceneGeometrySnapshot {
        uint64_t                           revision = 0;
        std::vector<SceneGeometryInstance> instances;
        // Version zero is a full external snapshot; deltas address this exact owned instances vector.
        uint64_t              version = 0, baseVersion = 0;
        std::vector<uint32_t> changed;
        std::vector<Uuid>     removed;
    };
    class ISceneResourceSource {
    public:
        virtual ~ISceneResourceSource() = default;
        // Pending is explicit; successful results own all CPU data and exact versions.
        virtual SceneResourceResult ResolveMesh(const Uuid &mesh, const Uuid &material, RendererInfo info) = 0;
        // Semantic changes advance this revision; initial asynchronous readiness does not.
        virtual uint64_t Revision() const
        {
            return 1;
        }
        virtual std::optional<AABB> MeshBounds(const Uuid &) const
        {
            return {};
        }
        virtual bool Invalidate(const Uuid &)
        {
            return false;
        }
        virtual SceneImageResult ResolveImage(const Uuid &, RendererInfo)
        {
            return RendererError{RendererErrorCode::UNSUPPORTED, "Scene source does not provide environment images"};
        }
    };

    class RenderSceneBridge {
    public:
        RenderSceneBridge(RendererInfo info, std::shared_ptr<ISceneResourceSource> resources);
        ~RenderSceneBridge();
        RenderSceneBridge(const RenderSceneBridge &)                                      = delete;
        RenderSceneBridge                           &operator=(const RenderSceneBridge &) = delete;
        void                                         SetWorld(World *world);
        void                                         EnableGeometrySnapshots(bool enabled);
        std::shared_ptr<const SceneGeometrySnapshot> GeometrySnapshot() const;
        // Resolve only live bridge identities; optional surface gating excludes unsupported highlight geometry.
        std::vector<RenderEntityRef> Entities(std::span<const Uuid> actors, bool staticOpaqueOnly = false) const;
        // The sole host seals/submits this packet after World::Tick; BUSY retains that same packet.
        std::optional<RendererError>      Append(FramePacket &packet, SceneId scene, ViewportId output = {});
        std::optional<ViewDesc>           Camera(SceneId scene, ViewportId output, Extent2D extent) const;
        void                              OnEvent(const RendererEvent &event);
        const std::vector<RendererError> &GetIssues() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
} // namespace sky::aurora
