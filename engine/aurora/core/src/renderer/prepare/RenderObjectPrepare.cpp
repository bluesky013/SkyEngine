#include "prepare/RenderObjectPrepare.h"

namespace sky::aurora {

    SystemDesc DescribeRenderObjectPrepare(SystemId id, SceneId scene)
    {
        SystemDesc system{id, "PrepareRenderObjects"};
        const auto owner = SceneAccessOwner(scene);
        system.accesses  = {ComponentAccess<RenderTransform>("scene", owner, AccessMode::READ),
                            ComponentAccess<RenderMeshBinding>("scene", owner, AccessMode::READ),
                            ComponentAccess<RenderMaterialBinding>("scene", owner, AccessMode::READ),
                            ComponentAccess<RenderVisibility>("scene", owner, AccessMode::READ),
                            ComponentAccess<RenderProperties>("scene", owner, AccessMode::READ),
                            ComponentAccess<RenderObjectData>("scene", owner, AccessMode::WRITE)};
        return system;
    }

    void PrepareRenderObjects(const FrozenPrepareContext &context, SceneId scene)
    {
        context.Write<RenderObjectData, RenderTransform, RenderMeshBinding, RenderMaterialBinding, RenderVisibility, RenderProperties>(scene)
            .ForEachDirty([](RenderEntityId, RenderObjectData::Value &object, const RenderTransform &transform, const RenderMeshBinding &mesh,
                             const RenderMaterialBinding &material, const RenderVisibility &visibility, const RenderProperties &properties) {
                object = {transform.world, mesh.binding, material.binding, visibility.mask, properties.value};
            });
    }

} // namespace sky::aurora
