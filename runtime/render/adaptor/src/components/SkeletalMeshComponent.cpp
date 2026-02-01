//
// Created by blues on 2024/8/11.
//

#include <render/adaptor/Util.h>
#include <render/adaptor/components/SkeletalMeshComponent.h>

#include <framework/serialization/PropertyCommon.h>
#include <framework/world/Actor.h>
#include <framework/world/TransformComponent.h>

#include <render/mesh/MeshFeatureProcessor.h>

#include <core/profile/Profiler.h>

namespace sky {
    SkeletalMeshComponent::~SkeletalMeshComponent()
    {
    }

    void SkeletalMeshComponent::Reflect(SerializationContext *context)
    {
        context->Register<SkeletonMeshComponentData>("SkeletonMeshComponentData")
            .Member<&SkeletonMeshComponentData::mesh>("Mesh");

        REGISTER_BEGIN(SkeletonMeshComponent, context)
            REGISTER_MEMBER(Mesh, SetMeshUuid, GetMeshUuid)
                SET_ASSET_TYPE(AssetTraits<Mesh>::ASSET_TYPE);
    }

    void SkeletalMeshComponent::Tick(float time)
    {
        if (isMeshDirty && renderer != nullptr) {
            renderer->SetMesh(cachedMesh);
            renderer->UpdateTransform(cachedTransform.ToMatrix());
            isMeshDirty = false;
        }
    }

    void SkeletalMeshComponent::OnAttachToWorld()
    {
        transformEvent.Bind(this, actor);
        poseEvent.Bind(this, actor);
        cachedTransform = actor->GetComponent<TransformComponent>()->GetWorldTransform();

        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        if (renderer == nullptr) {
            renderer = mf->CreateSkeletalMesh();
        }
    }

    void SkeletalMeshComponent::OnDetachFromWorld()
    {
        auto *mf = GetFeatureProcessor<MeshFeatureProcessor>(actor);
        if (renderer != nullptr) {
            mf->RemoveSkeletalMesh(renderer);
            renderer = nullptr;
        }

        transformEvent.Reset();
        poseEvent.Reset();
    }

    void SkeletalMeshComponent::OnSerialized()
    {
        SetMeshUuid(data.mesh);
    }

    void SkeletalMeshComponent::OnAssetLoaded(const Uuid& uuid, const std::string_view& type)
    {
        BuildSkeletonMeshAsync();
    }

    void SkeletalMeshComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        cachedTransform = global;

        if (renderer != nullptr) {
            renderer->UpdateTransform(cachedTransform.ToMatrix());
        }
    }

    static void UpdateBone(const Bone* bone, const AnimPose& pose, Skin& skin, const Matrix4* inverseBindMatrix, const Transform &world) // NOLINT
    {
        SKY_ASSERT(bone != nullptr);

        const auto &trans = pose.transforms[bone->index];
        Transform current = world * trans;

        skin.boneMatrices[bone->index] = current.ToMatrix() * inverseBindMatrix[bone->index];
        for (const auto& childIdx : bone->children) {
            const auto* child = pose.skeleton->GetBoneByIndex(childIdx);
            UpdateBone(child, pose, skin, inverseBindMatrix, current);
        }
    }

    void SkeletalMeshComponent::OnPoseUpdated(const AnimFinalPose& pose)
    {
        if (renderer == nullptr) {
            return;
        }

        const auto& bindSkin = renderer->GetSkin();
        const auto& bindMatrix = bindSkin->boneMatrices;

        Skin skinData = {};
        skinData.activeBone = bindSkin->activeBone;
        const auto& roots = pose.skeleton->GetRoots();
        for (const auto& root : roots) {
            UpdateBone(root, pose, skinData, bindMatrix.data(), Transform::GetIdentity());
        }

        renderer->UpdateSkinData(skinData);
    }

    void SkeletalMeshComponent::SetMeshUuid(const Uuid &uuid)
    {
        data.mesh = uuid;
        holder.SetAsset(uuid, this);
    }

    void SkeletalMeshComponent::BuildSkeletonMeshAsync()
    {
        SKY_PROFILE_NAME("Build SkeletalMeshRender")
        cachedMesh = CreateMeshFromAsset(holder.GetAsset(), true);
        isMeshDirty = true;
    }


} // namespace sky