//
// Created by blues on 2025/7/15.
//

#pragma once

#include <animation/Core/Skeleton.h>
#include <animation/Core/SkeletonAnimation.h>

#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <framework/world/Component.h>
#include <render/adaptor/assets/AnimationAsset.h>
#include <render/adaptor/assets/SkeletonAsset.h>
#include <render/adaptor/animation/SkeletonDebugRender.h>


namespace sky {
    class SerializationContext;

    struct AnimationPreviewData {
        Uuid clip;
    };

    class AnimationPreviewComponent
        : public ComponentAdaptor<AnimationPreviewData>
        , public ITransformEvent
        , public IAssetEvent {
    public:
        AnimationPreviewComponent() = default;
        ~AnimationPreviewComponent() override = default;

        COMPONENT_RUNTIME_INFO(AnimationPreviewComponent)

        static void Reflect(SerializationContext *context);

        void SetAnimationClip(const Uuid& uuid);
        const Uuid& GetAnimationClip() const { return data.clip; }

    private:
        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;
        void OnAssetLoaded() override;

        void Tick(float time) override;

        void UpdateAnimation(bool reset);

        SingleAssetHolder<Animation> clip;

        CounterPtr<SkeletonAnimation> animation;
        std::unique_ptr<SkeletonDebugRender> debugRender;

        EventBinder<ITransformEvent> transformEvent;

        bool dirty = false;
        Transform cachedTransform;
    };

} // namespace sky
