//
// Created by Zach Lee on 2023/2/28.
//

#include <render/adaptor/components/StaticMeshComponent.h>

#include <framework/asset/AssetManager.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>
#include <core/profile/Profiler.h>

#include <render/adaptor/Util.h>
#include <render/mesh/MeshFeatureProcessor.h>

namespace sky {

    StaticMeshComponent::~StaticMeshComponent()
    {
        ShutDown();
    }

    void StaticMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<StaticMeshComponent>("StaticMeshComponent")
            .Member<&StaticMeshComponent::SetMeshUuid, &StaticMeshComponent::GetMeshUuid>("Mesh")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(AssetTraits<Mesh>::ASSET_TYPE))
            .Member<&StaticMeshComponent::SetEnableMeshShading, &StaticMeshComponent::GetEnableMeshShading>("MeshShading")
            .Member<&StaticMeshComponent::SetMultiply, &StaticMeshComponent::GetMultiply>("Multiply")
            .Member<&StaticMeshComponent::SetEnableMeshletDebug, &StaticMeshComponent::GetEnableMeshletDebug>("DebugMeshlet")
            .Member<&StaticMeshComponent::SetEnableMeshletConeDebug, &StaticMeshComponent::GetEnableMeshletConeDebug>("DebugMeshletCone")
            .Member<&StaticMeshComponent::SetMeshDebug, &StaticMeshComponent::GetMeshDebug>("DebugMesh");
    }

    void StaticMeshComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.SaveValueObject(std::string("meshShading"), enableMeshShading);
        ar.SaveValueObject(std::string("multiply"), multiply);
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.EndObject();
    }

    void StaticMeshComponent::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
        ar.LoadKeyValue("meshShading", enableMeshShading);
        ar.LoadKeyValue("multiply", multiply);
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        SetMeshUuid(uuid);
    }

    void StaticMeshComponent::SetEnableMeshShading(bool enable)
    {
        enableMeshShading = enable;
        dirty.store(true);
    }

    void StaticMeshComponent::SetEnableMeshletDebug(bool enable)
    {
        if (enable) {
            debugFlags.SetBit(MeshDebugFlagBit::MESHLET);
        } else {
            debugFlags.ResetBit(MeshDebugFlagBit::MESHLET);
        }

        if (renderer != nullptr) {
            renderer->SetDebugFlags(debugFlags);
        }
    }

    void StaticMeshComponent::SetEnableMeshletConeDebug(bool enable)
    {
        if (enable) {
            debugFlags.SetBit(MeshDebugFlagBit::MESHLET_CONE);
        } else {
            debugFlags.ResetBit(MeshDebugFlagBit::MESHLET_CONE);
        }

        if (renderer != nullptr) {
            renderer->SetDebugFlags(debugFlags);
        }
    }

    void StaticMeshComponent::SetMeshDebug(bool enable)
    {
        if (enable) {
            debugFlags.SetBit(MeshDebugFlagBit::MESH);
        } else {
            debugFlags.ResetBit(MeshDebugFlagBit::MESH);
        }

        if (renderer != nullptr) {
            renderer->SetDebugFlags(debugFlags);
        }
    }

    void StaticMeshComponent::SetMultiply(bool enable)
    {
        multiply = enable;
        if (renderer != nullptr) {
            renderer->SetMesh(meshInstance, enableMeshShading);
//             if (multiply) {
// #if _WIN32
//                 renderer->BuildMultipleInstance(12, 12, 12);
// #else
//                 renderer->BuildMultipleInstance(8, 4, 4);
// #endif
//             }
        }
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
        dirty.store(true);

        meshAsset = uuid ? AssetManager::Get()->LoadAsset<Mesh>(uuid) : MeshAssetPtr {};
        if (meshAsset && meshAsset->IsLoaded() && !dirty.load()) {
            OnAssetLoaded();
        }
    }

    void StaticMeshComponent::BuildRenderer()
    {
        SKY_PROFILE_NAME("Build Static Render")
        if (meshAsset) {
            meshInstance = CreateMeshFromAsset(meshAsset);
        } else {
            meshInstance = nullptr;
        }

        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);

        if (renderer != nullptr) {
            mf->RemoveStaticMesh(renderer);
        }

        renderer = mf->CreateStaticMesh();
        // SetMultiply(multiply);
        renderer->SetMesh(meshInstance, enableMeshShading);
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