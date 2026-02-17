//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/asset/AssetEvent.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Mesh.h>
#include <render/mesh/MeshRenderer.h>
#include <render/lod/MeshLodGroup.h>

namespace sky {

    struct LodMeshAssetData {
        Uuid meshUuid;
        float screenSize = 0.f;
    };

    class StaticMeshComponent : public ComponentBase, public IAssetEvent {
    public:
        StaticMeshComponent() = default;
        ~StaticMeshComponent() override;

        COMPONENT_RUNTIME_INFO(StaticMeshComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

        MeshRenderer *GetRenderer() const { return renderer; }

        void SetMeshUuid(const Uuid &uuid);
        const Uuid& GetMeshUuid() const { return meshAsset ? meshAsset->GetUuid() : Uuid::GetEmpty(); }

        void SetEnableMeshShading(bool enable);
        bool GetEnableMeshShading() const { return enableMeshShading; }

        void SetEnableMeshletDebug(bool enable);
        bool GetEnableMeshletDebug() const { return debugFlags.TestBit(MeshDebugFlagBit::MESHLET); }

        void SetEnableMeshletConeDebug(bool enable);
        bool GetEnableMeshletConeDebug() const { return debugFlags.TestBit(MeshDebugFlagBit::MESHLET_CONE); }

        void SetLodMeshes(const std::vector<LodMeshAssetData> &lodMeshes);
        const std::vector<LodMeshAssetData> &GetLodMeshes() const { return lodMeshAssets; }
        void SetLodBias(float bias);
        float GetLodBias() const { return lodBias; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
    private:
        void ShutDown();
        void BuildRenderer();
        void BuildLodGroup();

        void OnAssetLoaded() override;

        bool isStatic = true;
        bool castShadow = false;
        bool receiveShadow = false;

        bool enableMeshShading = false;
        MeshDebugFlags debugFlags;

        MeshAssetPtr meshAsset;
        RDMeshPtr meshInstance;
        MeshRenderer *renderer = nullptr;

        std::vector<LodMeshAssetData> lodMeshAssets;
        std::vector<MeshAssetPtr> lodMeshAssetPtrs;
        RDMeshLodGroupPtr lodGroup;
        float lodBias = 1.0f;

        std::atomic_bool dirty = false;

        EventBinder<IAssetEvent, Uuid> binder;
    };

} // namespace receiveShadow
