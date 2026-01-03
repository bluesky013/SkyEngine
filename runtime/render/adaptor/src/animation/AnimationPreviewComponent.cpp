//
// Created by blues on 2025/7/15.
//

#include <render/adaptor/animation/AnimationPreviewComponent.h>
#include <render/adaptor/RenderSceneProxy.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <render/RenderTechniqueLibrary.h>
#include <animation/graph/AnimationClipNode.h>

namespace sky {

    void AnimationPreviewComponent::Reflect(SerializationContext *context)
    {
        context->Register<AnimationPreviewData>("AnimationPreviewData")
            .Member<&AnimationPreviewData::clip>("Clip");

        REGISTER_BEGIN(AnimationPreviewComponent, context)
            REGISTER_MEMBER(Clip, SetAnimationClip, GetAnimationClip)
                SET_ASSET_TYPE(AssetTraits<Animation>::ASSET_TYPE);
    }

    void AnimationPreviewComponent::SetAnimationClip(const Uuid& uuid)
    {
        data.clip = uuid;
        clip.SetAsset(uuid, this);
    }

    void AnimationPreviewComponent::OnTransformChanged(const Transform& global, const Transform& local)
    {
        dirty = true;
        cachedTransform = global;
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
        if (animation) {
            AnimationTick tick = {time};
            animation->Tick(tick);
        }
    }

    void AnimationPreviewComponent::UpdateAnimation(bool reset)
    {
        if (reset) {
            animation = nullptr;
            return;
        }

        auto &clipData = clip.Data();
        const auto &skeletonData = AssetManager::Get()->FindAsset<Skeleton>(clipData.skeleton)->Data();
        SkeletonPtr skl = Skeleton::BuildSkeleton(skeletonData);

        auto clipInst = CreateAnimationFromAsset(clip.GetAsset());

        animation = new SkeletonAnimation();

        SkeletonAnimationInit animInit = {};
        animInit.skeleton = skl;
        animInit.rootNode = animation->NewAnimNode<AnimationClipNode>(clipInst);
        animation->Init(animInit);
    }

} // namespace sky
