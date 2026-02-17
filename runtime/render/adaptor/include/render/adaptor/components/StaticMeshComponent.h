//
// Created by Zach Lee on 2023/2/28.
//

#pragma once

#include <framework/world/Component.h>
#include <framework/asset/AssetEvent.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Mesh.h>
#include <render/mesh/MeshRenderer.h>

namespace sky {

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

        void SetMeshDebug(bool enable);
        bool GetMeshDebug() const { return debugFlags.TestBit(MeshDebugFlagBit::MESH); }

        void SetMultiply(bool enable);
        bool GetMultiply() const { return multiply; }

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;
    private:
        void ShutDown();
        void BuildRenderer();

        void OnAssetLoaded() override;

        bool isStatic = true;
        bool castShadow = false;
        bool receiveShadow = false;

        bool enableMeshShading = false;
        MeshDebugFlags debugFlags;

        MeshAssetPtr meshAsset;
        RDMeshPtr meshInstance;
        MeshRenderer *renderer = nullptr;

        std::atomic_bool dirty = false;
        bool multiply = false;

        EventBinder<IAssetEvent, Uuid> binder;
    };

} // namespace receiveShadow
