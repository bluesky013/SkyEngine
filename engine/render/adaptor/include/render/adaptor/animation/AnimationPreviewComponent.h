//
// Created by blues on 2025/7/15.
//

#pragma once

#include <animation/Core/SkeletonAnimation.h>

#include <framework/asset/AssetHolder.h>
#include <framework/interface/ITransformEvent.h>
#include <framework/world/Component.h>

#include <render/adaptor/assets/AnimationAsset.h>
#include <render/adaptor/animation/SkeletonDebugRender.h>


namespace sky {
    class SerializationContext;

    class AnimationClipNode;

    struct AnimationPreviewData {
        Uuid clip;
    };

    class AnimationPreviewComponent
        : public ComponentAdaptor<AnimationPreviewData>
        , public ITransformEvent
        , public IAssetReadyNotifier {
    public:
        AnimationPreviewComponent() = default;
        ~AnimationPreviewComponent() override = default;

        COMPONENT_RUNTIME_INFO(AnimationPreviewComponent)

        static void Reflect(SerializationContext *context);

        void SetAnimationClip(const Uuid& uuid);
        const Uuid& GetAnimationClip() const { return data.clip; }

        void SetPlaying(bool playing);
        void SetLoop(bool loop);
        void SetEnableRootMotion(bool enabled);

        bool IsPlaying() const;
        bool IsLoop() const;
        bool IsRootMotionEnable() const;

    private:
        void OnTransformChanged(const Transform& global, const Transform& local) override;

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;
        void OnAssetLoaded(const Uuid& uuid, const std::string_view&) override;

        void Tick(float time) override;

        void UpdateAnimation(bool reset);

        SingleAssetHolder<AnimationClip> clip;

        AnimationClipNode *clipNode = nullptr;
        CounterPtr<SkeletonAnimation> animation;
        std::unique_ptr<SkeletonDebugRender> debugRender;

        EventBinder<ITransformEvent> transformEvent;

        // transient status data
        bool dirty = false;
        Transform cachedTransform;
        AnimFinalPose cachedPose;
    };

} // namespace sky
