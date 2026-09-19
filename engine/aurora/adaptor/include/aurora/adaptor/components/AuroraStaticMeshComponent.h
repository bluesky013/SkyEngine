//
// Aurora static mesh component: framework component layer view over aurora mesh
// + material assets (mirrors legacy StaticMeshComponent).
//

#pragma once

#include <aurora/adaptor/assets/MaterialAsset.h>
#include <aurora/adaptor/assets/MeshAsset.h>
#include <core/util/Uuid.h>
#include <framework/asset/AssetHolder.h>
#include <framework/world/Component.h>

namespace sky {

    struct AuroraStaticMeshData {
        Uuid mesh;
        Uuid material;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    class AuroraStaticMeshComponent : public ComponentAdaptor<AuroraStaticMeshData>, public IAssetReadyNotifier {
    public:
        AuroraStaticMeshComponent()           = default;
        ~AuroraStaticMeshComponent() override = default;

        COMPONENT_RUNTIME_INFO(AuroraStaticMeshComponent)

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

        void OnAssetLoaded(const Uuid &uuid, const std::string_view &type) override;

    private:
        SingleAssetHolder<sky::aurora::Mesh>     meshHolder;
        SingleAssetHolder<sky::aurora::Material> materialHolder;
    };

} // namespace sky
