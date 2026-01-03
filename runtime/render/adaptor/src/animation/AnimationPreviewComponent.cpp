//
// Created by blues on 2025/7/15.
//

#include <render/adaptor/animation/AnimationPreviewComponent.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/adaptor/assets/AnimationAsset.h>
#include <render/adaptor/RenderSceneProxy.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <render/RenderTechniqueLibrary.h>

#include <animation/core/Skeleton.h>
#include <animation/graph/AnimationClipNode.h>

namespace sky {

    void AnimationPreviewComponent::Reflect(SerializationContext *context)
    {
        context->Register<AnimationPreviewData>("AnimationPreviewData")
            .Member<&AnimationPreviewData::clip>("Clip");

        REGISTER_BEGIN(AnimationPreviewComponent, context)
            REGISTER_MEMBER(Clip, SetAnimationClip, GetAnimationClip)
                SET_ASSET_TYPE(AssetTraits<Animation>::ASSET_TYPE)
            REGISTER_MEMBER(Loop, SetLoop, IsLoop)
            REGISTER_MEMBER(Play, SetPlaying, IsPlaying)
            REGISTER_MEMBER(RootMotion, SetEnableRootMotion, IsRootMotionEnable);
    }

    void AnimationPreviewComponent::SetPlaying(bool inPlaying)
    {
        if (clipNode != nullptr) {
            clipNode->SetPlaying(inPlaying);
        }
    }

    void AnimationPreviewComponent::SetLoop(bool inLoop)
    {
        if (clipNode != nullptr) {
            clipNode->SetLooping(inLoop);
        }
    }

    void AnimationPreviewComponent::SetEnableRootMotion(bool enable)
    {
        if (clipNode != nullptr) {
            clipNode->SetEnableRootMotion(enable);;
        }
    }

    bool AnimationPreviewComponent::IsPlaying() const
    {
        return clipNode != nullptr ? clipNode->IsPlaying() : false;
    }

    bool AnimationPreviewComponent::IsLoop() const
    {
        return clipNode != nullptr ? clipNode->IsLooping() : false;
    }

    bool AnimationPreviewComponent::IsRootMotionEnable() const
    {
        return clipNode != nullptr ? clipNode->IsRootMotionEnable() : false;
    }


    void AnimationPreviewComponent::SetAnimationClip(const Uuid& uuid)
    {
        data.clip = uuid;
        clip.SetAsset(uuid, this);
    }

    void AnimationPreviewComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        cachedTransform = global;
        dirty = true;
    }

    void AnimationPreviewComponent::OnAttachToWorld()
    {
        if (!debugRender) {
            debugRender = std::make_unique<SkeletonDebugRender>();
            debugRender->SetTechnique(RenderTechniqueLibrary::Get()->FetchGfxTechnique(Name("techniques/debug.tech")));
        }

        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        renderScene->AddPrimitive(debugRender->GetPrimitive());

        dirty = true;
        transformEvent.Bind(this, actor);
        cachedTransform = actor->GetComponent<TransformComponent>()->GetWorldTransform();
    }

    void AnimationPreviewComponent::OnDetachFromWorld()
    {
        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        renderScene->RemovePrimitive(debugRender->GetPrimitive());

        transformEvent.Reset();
    }

    void AnimationPreviewComponent::OnSerialized()
    {
        SetAnimationClip(data.clip);
    }

    void AnimationPreviewComponent::OnAssetLoaded()
    {
        UpdateAnimation(!clip.IsLoaded());
    }

    void AnimationPreviewComponent::Tick(float time)
    {
        if (!animation) {
            return;
        }

        AnimationTick tick = {time};
        animation->Tick(tick);

        AnimationEval eval(animation->GetSkeleton());
        animation->EvalAny(eval);
        debugRender->DrawPose(eval.pose, cachedTransform);
    }

    void AnimationPreviewComponent::UpdateAnimation(bool reset)
    {
        if (reset) {
            animation = nullptr;
            return;
        }

        AnimationClipNode::PersistentData initData = {};
        initData.clip = CreateAnimationFromAsset(clip.GetAsset());
        initData.looping = IsLoop();
        initData.rootMotion = IsRootMotionEnable();

        auto &clipData = clip.Data();
        const auto &skeletonData = AssetManager::Get()->FindAsset<Skeleton>(clipData.skeleton)->Data();
        SkeletonPtr skl = Skeleton::BuildSkeleton(skeletonData);
        cachedPose.SetSkeleton(skl);
        animation = new SkeletonAnimation();
        clipNode = animation->NewAnimNode<AnimationClipNode>(initData);
        SkeletonAnimationInit animInit = {};
        animInit.skeleton = skl;
        animInit.rootNode = clipNode;
        animation->Init(animInit);

        dirty = true;
    }

} // namespace sky
