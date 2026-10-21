#pragma once

#include <aurora/renderer/RenderCommands.h>
#include <memory>

namespace sky {
    class World;
}

namespace sky::aurora {
    struct SceneResourcePart {
        uint32_t              part = 0;
        RenderResourceBinding mesh, material;
    };
    struct SceneMeshResources {
        std::vector<SceneResourcePart>     parts;
        std::vector<RenderResourcePayload> payloads;
    };
    using SceneResourceResult = std::variant<std::monostate, RendererError, std::shared_ptr<const SceneMeshResources>>;
    using SceneImageResult    = std::variant<std::monostate, RendererError, std::shared_ptr<const RenderResourcePayload>>;
    class ISceneResourceSource {
    public:
        virtual ~ISceneResourceSource() = default;
        // Pending is explicit; successful results own all CPU data and exact versions.
        virtual SceneResourceResult ResolveMesh(const Uuid &mesh, const Uuid &material, RendererInfo info) = 0;
        virtual SceneImageResult    ResolveImage(const Uuid &, RendererInfo)
        {
            return RendererError{RendererErrorCode::UNSUPPORTED, "Scene source does not provide environment images"};
        }
    };

    class RenderSceneBridge {
    public:
        RenderSceneBridge(RendererInfo info, std::shared_ptr<ISceneResourceSource> resources);
        ~RenderSceneBridge();
        RenderSceneBridge(const RenderSceneBridge &)            = delete;
        RenderSceneBridge &operator=(const RenderSceneBridge &) = delete;
        void               SetWorld(World *world);
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
