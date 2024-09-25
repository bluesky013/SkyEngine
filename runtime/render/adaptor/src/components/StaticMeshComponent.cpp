//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/StaticMeshComponent.h>
#include <render/adaptor/Util.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    StaticMeshComponent::~StaticMeshComponent()
    {
        ShutDown();
    }

    void StaticMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<StaticMeshComponent>("StaticMeshComponent")
            .Member<&StaticMeshComponent::isStatic>("static")
            .Member<&StaticMeshComponent::SetMeshUuid, &StaticMeshComponent::GetMeshUuid>("mesh")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Mesh>::ASSET_TYPE));
    }

    void StaticMeshComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.EndObject();
    }

    void StaticMeshComponent::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        SetMeshUuid(uuid);
    }

    void StaticMeshComponent::SetMeshUuid(const Uuid &uuid)
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

    void StaticMeshComponent::BuildRenderer()
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
            renderer = mf->CreateStaticMesh();
        }
        renderer->SetMesh(meshInstance);
    }

    void StaticMeshComponent::ShutDown()
    {
        if (renderer != nullptr) {
            GetFeatureProcessor<MeshFeatureProcessor>(actor)->RemoveStaticMesh(renderer);
            renderer = nullptr;
        }
    }

    void StaticMeshComponent::OnAssetLoaded()
    {
        dirty.store(true);
    }

    void StaticMeshComponent::Tick(float time)
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

    void StaticMeshComponent::OnAttachToWorld()
    {

    }

    void StaticMeshComponent::OnDetachFromWorld()
    {
        ShutDown();
    }
} // namespace sky