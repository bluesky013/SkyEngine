//
// Static mesh component: framework component-layer view over aurora mesh +
// material assets (mirrors legacy StaticMeshComponent).
//

#pragma once

#include <aurora/adaptor/components/AuroraComponentData.h>
#include <aurora/adaptor/renderer/SceneComponentEvent.h>

namespace sky::aurora {

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
        const Uuid &GetMeshUuid() const
        {
            return data.mesh;
        }

        // Sets the slot 0 override; empty clears it so slot 0 falls back to the
        // mesh asset's materials[0].
        void SetMaterialUuid(const Uuid &uuid)
        {
            data.material = uuid;
        }
        const Uuid &GetMaterialUuid() const
        {
            return data.material;
        }

        void SetCastShadow(bool value)
        {
            data.castShadow = value;
        }
        bool GetCastShadow() const
        {
            return data.castShadow;
        }

        void SetReceiveShadow(bool value)
        {
            data.receiveShadow = value;
        }
        bool GetReceiveShadow() const
        {
            return data.receiveShadow;
        }
    };

} // namespace sky::aurora
