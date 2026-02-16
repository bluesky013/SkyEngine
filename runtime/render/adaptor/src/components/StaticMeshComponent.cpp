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
            .Member<&StaticMeshComponent::SetEnableMeshletDebug, &StaticMeshComponent::GetEnableMeshletDebug>("DebugMeshlet")
            .Member<&StaticMeshComponent::SetEnableMeshletConeDebug, &StaticMeshComponent::GetEnableMeshletConeDebug>("DebugMeshletCone");
    }

    void StaticMeshComponent::SaveJson(JsonOutputArchive &ar) const
    {
        ar.StartObject();
        ar.SaveValueObject(std::string("static"), isStatic);
        ar.SaveValueObject(std::string("castShadow"), castShadow);
        ar.SaveValueObject(std::string("receiveShadow"), receiveShadow);
        ar.SaveValueObject(std::string("meshShading"), enableMeshShading);
        ar.SaveValueObject(std::string("mesh"), meshAsset ? meshAsset->GetUuid() : Uuid());
        ar.SaveValueObject(std::string("lodBias"), lodBias);
        ar.SaveValueObject(std::string("lodPolicy"), static_cast<uint32_t>(lodPolicy));

        ar.Key("lodMeshes");
        ar.StartArray();
        for (const auto &lod : lodMeshAssets) {
            ar.StartObject();
            ar.SaveValueObject(std::string("mesh"), lod.meshUuid);
            ar.SaveValueObject(std::string("screenSize"), lod.screenSize);
            ar.SaveValueObject(std::string("distance"), lod.distance);
            ar.EndObject();
        }
        ar.EndArray();

        ar.EndObject();
    }

    void StaticMeshComponent::LoadJson(JsonInputArchive &ar)
    {
        ar.LoadKeyValue("static", isStatic);
        ar.LoadKeyValue("castShadow", castShadow);
        ar.LoadKeyValue("receiveShadow", receiveShadow);
        ar.LoadKeyValue("meshShading", enableMeshShading);
        Uuid uuid;
        ar.LoadKeyValue("mesh", uuid);
        SetMeshUuid(uuid);
        ar.LoadKeyValue("lodBias", lodBias);
        uint32_t policyVal = 0;
        ar.LoadKeyValue("lodPolicy", policyVal);
        lodPolicy = static_cast<LodPolicy>(policyVal);
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

    void StaticMeshComponent::SetLodMeshes(const std::vector<LodMeshAssetData> &lodMeshes)
    {
        lodMeshAssets = lodMeshes;
        dirty.store(true);
    }

    void StaticMeshComponent::SetLodBias(float bias)
    {
        lodBias = bias;
        if (lodGroup) {
            lodGroup->SetLodBias(lodBias);
        }
    }

    void StaticMeshComponent::SetLodPolicy(LodPolicy policy)
    {
        lodPolicy = policy;
        if (lodGroup) {
            lodGroup->SetLodPolicy(lodPolicy);
        }
    }

    void StaticMeshComponent::BuildLodGroup()
    {
        if (lodMeshAssets.empty()) {
            lodGroup = nullptr;
            return;
        }

        auto *am = AssetManager::Get();
        lodGroup = new MeshLodGroup();
        lodGroup->SetLodBias(lodBias);
        lodGroup->SetLodPolicy(lodPolicy);
        lodMeshAssetPtrs.clear();

        for (const auto &lodData : lodMeshAssets) {
            auto asset = am->LoadAsset<Mesh>(lodData.meshUuid);
            if (asset) {
                asset->BlockUntilLoaded();
                auto meshPtr = CreateMeshFromAsset(asset);
                lodGroup->AddLodMesh(meshPtr, lodData.screenSize, lodData.distance);
                lodMeshAssetPtrs.emplace_back(asset);
            }
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

        BuildLodGroup();

        if (lodGroup && lodGroup->GetLodCount() > 0) {
            renderer->SetLodGroup(lodGroup);
        } else {
            renderer->SetMesh(meshInstance, enableMeshShading);
        }
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