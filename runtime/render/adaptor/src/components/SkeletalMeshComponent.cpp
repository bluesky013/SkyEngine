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

    static void UpdateBone(const Bone* bone, const AnimPose& pose, Matrix4* outMatrix, const Transform &world) // NOLINT
    {
        SKY_ASSERT(bone != nullptr);

        const auto &trans = pose.transforms[bone->index];
        Transform current = world * trans;

        outMatrix[bone->index] = current.ToMatrix();
        for (const auto& childIdx : bone->children) {
            const auto* child = pose.skeleton->GetBoneByIndex(childIdx);
            assert(child->index == childIdx);
            UpdateBone(child, pose, outMatrix, current);
        }
    }

    void SkeletalMeshComponent::OnPoseUpdated(const AnimFinalPose& pose)
    {
        if (renderer == nullptr) {
            return;
        }

        const auto& roots = pose.skeleton->GetRoots();
        std::vector<Matrix4> boneMatrices(pose.skeleton->GetNumBones());
        for (const auto& root : roots) {
            UpdateBone(root, pose, boneMatrices.data(), cachedTransform);
        }

        auto numSection = renderer->GetNumSubMeshes();
        for (uint32_t section = 0; section < numSection; section++) {
            const auto& bindSkin = renderer->GetSkin(section);
            const auto& bindMatrix = bindSkin->boneMatrices;
            SkinPtr skinData = new Skin();
            skinData->boneMapping = bindSkin->boneMapping;
            for (uint32_t index = 0; index <bindSkin->boneMapping.size(); ++index) {
                skinData->boneMatrices[index] = boneMatrices[bindSkin->boneMapping[index]] * bindMatrix[index];
            }

            renderer->UpdateSkinData(*skinData, section);
        }
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