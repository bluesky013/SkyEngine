//
// Lod group component: framework component-layer view over an aurora lod group
// asset (mirrors the legacy LodGroup reference on StaticMeshComponent).
//

#pragma once

#include <aurora/adaptor/assets/LodGroupAsset.h>
#include <core/util/Uuid.h>
#include <framework/asset/AssetHolder.h>
#include <framework/world/Component.h>

namespace sky::aurora {

    struct LodGroupComponentData {
        Uuid lodGroup;
        bool castShadow    = false;
        bool receiveShadow = false;
    };

    class LodGroupComponent : public ComponentAdaptor<LodGroupComponentData>, public IAssetReadyNotifier {
    public:
        LodGroupComponent()           = default;
        ~LodGroupComponent() override = default;

        COMPONENT_RUNTIME_INFO(LodGroupComponent)

        static void Reflect(SerializationContext *context);

        void SetLodGroupUuid(const Uuid &uuid)
        {
            data.lodGroup = uuid;
            lodGroupHolder.SetAsset(uuid, this);
        }
        const Uuid &GetLodGroupUuid() const { return data.lodGroup; }

        void SetCastShadow(bool value) { data.castShadow = value; }
        bool GetCastShadow() const { return data.castShadow; }

        void SetReceiveShadow(bool value) { data.receiveShadow = value; }
        bool GetReceiveShadow() const { return data.receiveShadow; }

        // Loaded asset payload (null until the async load completes).
        Asset<sky::aurora::LodGroup> *GetLodGroupAsset() const { return lodGroupHolder.GetAsset().get(); }
        bool                          IsLodGroupLoaded() const { return lodGroupHolder.IsLoaded(); }

        void OnAssetLoaded(const Uuid &uuid, const std::string_view &type) override;

    private:
        SingleAssetHolder<sky::aurora::LodGroup> lodGroupHolder;
    };

} // namespace sky::aurora
