//
// Created by blues on 2024/8/11.
//

#include <render/adaptor/components/SkeletonMeshComponent.h>
#include <render/adaptor/Util.h>

#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {
    SkeletonMeshComponent::~SkeletonMeshComponent()
    {
        ShutDown();
    }

    void SkeletonMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<SkeletonMeshComponent>("SkeletonMeshComponent")
            .Member<&SkeletonMeshComponent::SetMeshUuid, &SkeletonMeshComponent::GetMeshUuid>("mesh")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Mesh>::ASSET_TYPE));
    }

    void SkeletonMeshComponent::Tick(float time)
    {
        if (dirty.load()) {
            BuildRenderer();
            dirty.store(false);
        }

        if (renderer != nullptr) {
            auto *ts = actor->GetComponent<TransformComponent>();
            renderer->UpdateTransform(ts->GetWorldMatrix());
        }
    }

    void SkeletonMeshComponent::BuildRenderer()
    {
        if (meshAsset) {
            meshInstance = CreateMeshFromAsset(meshAsset);
        } else {
            meshInstance = nullptr;
        }

        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);

        if (!meshInstance) {
            if (renderer != nullptr) {
                mf->RemoveStaticMesh(renderer);
            }
            renderer = nullptr;
            return;
        }

        if (renderer == nullptr) {
            renderer = mf->CreateSkeletonMesh();
        }
        renderer->SetMesh(meshInstance);
    }

    void SkeletonMeshComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.EndObject();
    }

    void SkeletonMeshComponent::LoadJson(JsonInputArchive &ar)
    {
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        SetMeshUuid(uuid);
    }

    void SkeletonMeshComponent::OnAttachToWorld()
    {

    }

    void SkeletonMeshComponent::OnDetachFromWorld()
    {
        ShutDown();
    }

    void SkeletonMeshComponent::SetMeshUuid(const Uuid &uuid)
    {
        const auto &current = meshAsset ? meshAsset->GetUuid() : Uuid::GetEmpty();
        if (current == uuid) {
            return;
        }

        if (uuid) {
            binder.Bind(this, uuid);
        } else {
            binder.Reset();
        }
        dirty.store(false);

        meshAsset = uuid ? AssetManager::Get()->LoadAsset<Mesh>(uuid) : MeshAssetPtr {};
        if (meshAsset && meshAsset->IsLoaded() && !dirty.load()) {
            OnAssetLoaded();
        }
    }

    void SkeletonMeshComponent::ShutDown()
    {
        if (renderer != nullptr) {
            GetFeatureProcessor<MeshFeatureProcessor>(actor)->RemoveStaticMesh(renderer);
            renderer = nullptr;
        }
    }

    void SkeletonMeshComponent::OnAssetLoaded()
    {
        dirty.store(true);
    }

} // namespace sky