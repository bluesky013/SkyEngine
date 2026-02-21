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

    void StaticMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<StaticMeshComponentData>("StaticMeshComponentData")
            .Member<&StaticMeshComponentData::castShadow>("CastShadow")
            .Member<&StaticMeshComponentData::receiveShadow>("ReceiveShadow")
            .Member<&StaticMeshComponentData::resId>("LodMesh");

        REGISTER_BEGIN(StaticMeshComponent, context)
            REGISTER_MEMBER(LodMesh, SetMeshUuid, GetMeshUuid)
                SET_ASSET_TYPE(AssetTraits<LodGroup>::ASSET_TYPE);
    }

    void StaticMeshComponent::SetMeshUuid(const Uuid &uuid)
    {
        data.resId = uuid;
        holder.SetAsset(uuid, this);
    }

    void StaticMeshComponent::Tick(float time)
    {
        if (isMeshDirty && renderer != nullptr) {
            renderer->SetMeshLodGroup(cachedGroup);
            renderer->UpdateTransform(cachedTransform.ToMatrix());
            isMeshDirty = false;
        }
    }

    void StaticMeshComponent::OnAttachToWorld()
    {
        transformEvent.Bind(this, actor);
        cachedTransform = actor->GetComponent<TransformComponent>()->GetWorldTransform();

        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        if (renderer == nullptr) {
            renderer = mf->CreateStaticMesh();
        }
    }

    void StaticMeshComponent::OnDetachFromWorld()
    {
        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        if (renderer != nullptr) {
            mf->RemoveStaticMesh(renderer);
            renderer = nullptr;
        }

        transformEvent.Reset();
    }

    void StaticMeshComponent::OnSerialized()
    {
        SetMeshUuid(data.resId);
    }

    void StaticMeshComponent::OnAssetLoaded(const Uuid& uuid, const std::string_view& type)
    {
        BuildLodGroupAsync();
    }

    void StaticMeshComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        cachedTransform = global;

        if (renderer != nullptr) {
            renderer->UpdateTransform(cachedTransform.ToMatrix());
        }
    }

    void StaticMeshComponent::BuildLodGroupAsync()
    {
        SKY_PROFILE_NAME("Build SkeletalMeshRender")
        cachedGroup = CreateLodGroupFromAsset(holder.GetAsset()).group;
        isMeshDirty = true;
    }
} // namespace sky