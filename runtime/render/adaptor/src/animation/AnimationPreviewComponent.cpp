//
// Created by blues on 2025/7/15.
//

#include <render/adaptor/animation/AnimationPreviewComponent.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <framework/serialization/SerializationContext.h>
#include <render/RenderTechniqueLibrary.h>

namespace sky {

    void AnimationPreviewComponent::Reflect(SerializationContext *context)
    {
        context->Register<AnimationPreviewData>("AnimationPreviewData")
            .Member<&AnimationPreviewData::clip>("Clip")
            .Member<&AnimationPreviewData::skeleton>("Skeleton");

        REGISTER_BEGIN(AnimationPreviewComponent, context)
            REGISTER_MEMBER(Clip, SetAnimationClip, GetAnimationClip)
                SET_ASSET_TYPE(AssetTraits<Animation>::ASSET_TYPE)
            REGISTER_MEMBER(Skeleton, SetSkeleton, GetSkeleton)
                SET_ASSET_TYPE(AssetTraits<Skeleton>::ASSET_TYPE);
    }

    void AnimationPreviewComponent::SetAnimationClip(const Uuid& uuid)
    {
        data.clip = uuid;
        clip.SetAsset(uuid, this);
    }

    void AnimationPreviewComponent::SetSkeleton(const Uuid& uuid)
    {
        data.skeleton = uuid;
        skeleton.SetAsset(uuid, this);
    }

    void AnimationPreviewComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {

    }

    void AnimationPreviewComponent::OnSerialized()
    {
        SetAnimationClip(data.clip);
        SetSkeleton(data.skeleton);
    }

    void AnimationPreviewComponent::OnAssetLoaded()
    {
        bool isReady = skeleton.IsLoaded() && clip.IsLoaded();
        UpdateAnimation(!isReady);
    }

    void AnimationPreviewComponent::Tick(float time)
    {
        if (animation) {
            animation->Tick(time);

            debugRender->DrawPose(animation->GetCurrentPose(), Transform::GetIdentity());
//            debugRender->DrawPose(animation->GetSkeleton()->GetRefPos(), Transform::GetIdentity());
        }
    }

    void AnimationPreviewComponent::UpdateAnimation(bool reset)
    {
        if (reset) {
            animation = nullptr;
            return;
        }

        auto &skeletonData = skeleton.Data();
        SkeletonPtr skl = Skeleton::BuildSkeleton(skeletonData);

        if (!debugRender) {
            debugRender = std::make_unique<SkeletonDebugRender>();
            debugRender->SetTechnique(RenderTechniqueLibrary::Get()->FetchGfxTechnique(Name("techniques/debug.tech")));
            auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
            renderScene->AddPrimitive(debugRender->GetPrimitive());
        }

        animation = new SkeletonAnimation(skl);
        auto clipInst = CreateAnimationFromAsset(clip.GetAsset());
        animation->AddClip(clipInst);
    }

} // namespace sky
