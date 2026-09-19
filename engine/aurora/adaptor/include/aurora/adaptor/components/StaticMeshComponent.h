//
// Static mesh component: framework component-layer view over aurora mesh +
// material assets (mirrors legacy StaticMeshComponent).
//

#pragma once

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <core/util/Uuid.h>
#include <framework/asset/AssetHolder.h>
#include <framework/world/Component.h>

namespace sky::aurora {

    struct StaticMeshComponentData {
        Uuid mesh;
        Uuid material;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    class StaticMeshComponent : public ComponentAdaptor<StaticMeshComponentData>, public IAssetReadyNotifier {
    public:
        StaticMeshComponent()           = default;
        ~StaticMeshComponent() override = default;

        COMPONENT_RUNTIME_INFO(StaticMeshComponent)

        static void Reflect(SerializationContext *context);

        void SetMeshUuid(const Uuid &uuid)
        {
            data.mesh = uuid;
            meshHolder.SetAsset(uuid, this);
        }
        const Uuid &GetMeshUuid() const { return data.mesh; }

        void SetMaterialUuid(const Uuid &uuid)
        {
            data.material = uuid;
            materialHolder.SetAsset(uuid, this);
        }
        const Uuid &GetMaterialUuid() const { return data.material; }

        void SetCastShadow(bool value) { data.castShadow = value; }
        bool GetCastShadow() const { return data.castShadow; }

        void SetReceiveShadow(bool value) { data.receiveShadow = value; }
        bool GetReceiveShadow() const { return data.receiveShadow; }

        // Loaded asset payloads (null until the async load completes).
        Asset<sky::aurora::Mesh>     *GetMeshAsset() const { return meshHolder.GetAsset().get(); }
        Asset<sky::aurora::Material> *GetMaterialAsset() const { return materialHolder.GetAsset().get(); }
        bool                          IsMeshLoaded() const { return meshHolder.IsLoaded(); }
        bool                          IsMaterialLoaded() const { return materialHolder.IsLoaded(); }

        void OnAssetLoaded(const Uuid &uuid, const std::string_view &type) override;

    private:
        SingleAssetHolder<sky::aurora::Mesh>     meshHolder;
        SingleAssetHolder<sky::aurora::Material> materialHolder;
    };

} // namespace sky::aurora
