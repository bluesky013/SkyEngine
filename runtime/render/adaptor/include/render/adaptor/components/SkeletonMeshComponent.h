//
// Created by blues on 2024/8/11.
//

#pragma once


#include <framework/world/Component.h>
#include <framework/asset/AssetEvent.h>
#include <render/adaptor/assets/MeshAsset.h>
#include <render/resource/Mesh.h>
#include <render/skeleton/SkeletonMeshRenderer.h>

namespace sky {

    class SkeletonMeshComponent : public ComponentBase, public IAssetEvent {
    public:
        SkeletonMeshComponent() = default;
        ~SkeletonMeshComponent() override;

        COMPONENT_RUNTIME_INFO(SkeletonMeshComponent)

        static void Reflect(SerializationContext *context);

        void Tick(float time) override;
        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void SaveJson(JsonOutputArchive &ar) const override;
        void LoadJson(JsonInputArchive &ar) override;

        void SetMeshUuid(const Uuid &uuid);
        const Uuid& GetMeshUuid() const { return meshAsset ? meshAsset->GetUuid() : Uuid::GetEmpty(); }
    private:
        void ShutDown();
        void BuildRenderer();

        void OnAssetLoaded() override;

        MeshAssetPtr meshAsset;
        RDMeshPtr meshInstance;
        SkeletonMeshRenderer *renderer = nullptr;

        std::atomic_bool dirty = false;
        EventBinder<IAssetEvent, Uuid> binder;
    };

} // namespace receiveShadow
