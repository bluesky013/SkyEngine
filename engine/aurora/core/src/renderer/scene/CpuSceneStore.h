#pragma once

#include "scene/DeferredSceneCommands.h"
#include <aurora/renderer/RenderCommands.h>
#include <aurora/rhi/SwapChain.h>
#include <core/ecs/EntityRegistry.h>
#include <map>
#include <memory>
#include <optional>

namespace sky::aurora {

    class FrozenCpuFrame;

    struct RenderTransform {
        Matrix4 world = RenderIdentityMatrix();
    };
    struct RenderMeshBinding {
        RenderResourceBinding binding;
    };
    struct RenderMaterialBinding {
        RenderResourceBinding binding;
    };
    struct RenderVisibility {
        uint64_t mask = ~uint64_t{0};
    };
    struct RenderProperties {
        RenderDrawProperties value;
    };
    struct RenderObjectData {
        struct Value {
            Matrix4                                    world = RenderIdentityMatrix();
            RenderResourceBinding                      mesh;
            RenderResourceBinding                      material;
            uint64_t                                   visibilityMask = 0;
            RenderDrawProperties                       properties;
            std::shared_ptr<const ResourceVersionData> selectedMesh;
            std::shared_ptr<const ResourceVersionData> selectedMaterial;
            std::optional<AABB>                        worldBounds;
            std::optional<AABB>                        lightBounds;
        } value;
        uint64_t sourceRevision   = 0;
        uint64_t preparedSequence = 0;
        bool     valid            = false;
    };

    struct RenderEntityState {
        RenderEntityId id;
        EntityId       entity   = INVALID_ENTITY;
        uint64_t       revision = 0;
    };

    struct CpuSceneState {
        SceneId                               id;
        SceneDesc                             desc;
        uint64_t                              lastChangedSequence = 0;
        EntityRegistry                        registry;
        std::map<uint32_t, RenderEntityState> entities;

        const RenderEntityState *FindEntity(RenderEntityId entity) const;
    };

    struct CpuViewState {
        ViewId   id;
        ViewDesc desc;
        uint64_t revision = 0;
    };

    struct CpuViewportState {
        ViewportId                          id;
        ViewportDesc                        desc;
        uint64_t                            revision = 0;
        std::optional<SurfaceConfiguration> surface;
    };

    enum class CommandSkipReason : uint8_t { STALE_HANDLE, STALE_REVISION, RETIREMENT_PENDING };
    enum class CommandSource : uint8_t { PACKET, DEFERRED };
    struct SkippedRenderCommand {
        size_t            commandIndex = 0;
        CommandSkipReason reason       = CommandSkipReason::STALE_HANDLE;
        CommandSource     source       = CommandSource::PACKET;
    };

    // CPU removal only; the coordinator must retire borrowers/GPU/presentation before publishing a destroy acknowledgement.
    using CpuRetirement = std::variant<SceneId, RenderEntityRef, ViewId, ViewportId>;

    struct PacketApplyResult {
        uint64_t                          sequence = 0;
        std::optional<RendererError>      error;
        std::vector<SkippedRenderCommand> skipped;
        std::vector<CpuRetirement>        retirements;
        std::vector<SceneId>              changedScenes;
        uint64_t                          deferredSequence = 0;
        size_t                            deferredCommands = 0;
        bool                              Succeeded() const
        {
            return !error.has_value();
        }
    };

    struct CpuPrepareSelection {
        std::vector<SceneId> scenes;
        std::vector<ViewId>  views;
    };

    // Structural mutations and raw queries belong to the coordinator; kernels receive restricted frozen contexts.
    class CpuSceneStore {
    public:
        bool SetViewportSurface(ViewportId, const SurfaceConfiguration &);
        CpuSceneStore();
        ~CpuSceneStore();
        CpuSceneStore(const CpuSceneStore &)                     = delete;
        CpuSceneStore          &operator=(const CpuSceneStore &) = delete;
        PacketApplyResult       Apply(const FramePacket &packet, const DeferredSceneBatch *deferred = nullptr, bool acceptResources = false);
        bool                    AcknowledgeRetirement(const CpuRetirement &retirement);
        const CpuSceneState    *FindScene(SceneId id) const;
        const CpuViewState     *FindView(ViewId id) const;
        const CpuViewportState *FindViewport(ViewportId id) const;
        CpuPrepareSelection     SelectPrepareInputs() const;
        bool                    IsFailed() const;
        std::shared_ptr<const FrozenCpuFrame> Freeze();
        bool                                  IsFrozen() const;
        void                                  WaitUntilUnfrozen() const;

    private:
        struct Storage;
        std::shared_ptr<Storage> storage;
    };

} // namespace sky::aurora

SKY_TYPE_TAG(sky::aurora::RenderTransform, "sky.aurora.renderer.RenderTransform")
SKY_TYPE_TAG(sky::aurora::RenderMeshBinding, "sky.aurora.renderer.RenderMeshBinding")
SKY_TYPE_TAG(sky::aurora::RenderMaterialBinding, "sky.aurora.renderer.RenderMaterialBinding")
SKY_TYPE_TAG(sky::aurora::RenderVisibility, "sky.aurora.renderer.RenderVisibility")
SKY_TYPE_TAG(sky::aurora::RenderProperties, "sky.aurora.renderer.RenderProperties")
SKY_TYPE_TAG(sky::aurora::RenderObjectData, "sky.aurora.renderer.RenderObjectData")
