//
// Static mesh component: framework component-layer view over aurora mesh +
// material assets (mirrors legacy StaticMeshComponent).
//

#pragma once

#include <aurora/adaptor/renderer/SceneComponentEvent.h>
#include <core/util/Uuid.h>

namespace sky::aurora {

    struct StaticMeshComponentData {
        Uuid mesh;
        // Optional override for material slot 0. Empty means the mesh asset's
        // materials[0]; non-empty only overrides slot 0 -- sub-meshes with
        // materialIndex != 0 keep the mesh asset's materials[materialIndex].
        Uuid material;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    class StaticMeshComponent : public SceneComponentAdaptor<StaticMeshComponentData> {
    public:
        StaticMeshComponent()           = default;
        ~StaticMeshComponent() override = default;

        COMPONENT_RUNTIME_INFO(StaticMeshComponent)

        static void Reflect(SerializationContext *context);

        void SetMeshUuid(const Uuid &uuid)
        {
            data.mesh = uuid;
        }
        const Uuid &GetMeshUuid() const { return data.mesh; }

        // Sets the slot 0 override; empty clears it so slot 0 falls back to the
        // mesh asset's materials[0].
        void SetMaterialUuid(const Uuid &uuid)
        {
            data.material = uuid;
        }
        const Uuid &GetMaterialUuid() const { return data.material; }

        void SetCastShadow(bool value) { data.castShadow = value; }
        bool GetCastShadow() const { return data.castShadow; }

        void SetReceiveShadow(bool value) { data.receiveShadow = value; }
        bool GetReceiveShadow() const { return data.receiveShadow; }
    };

} // namespace sky::aurora
