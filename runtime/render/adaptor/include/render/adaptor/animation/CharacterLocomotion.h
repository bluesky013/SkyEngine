//
// Created by Zach Lee on 2026/1/4.
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

    enum class CharacterStandardAction : uint8_t {
        IDLE,
        WALK_FWD,
        WALK_BACK,
        WALK_LEFT,
        WALK_RIGHT,
        RUN,
        JUMP,
        NUM
    };

    struct CharacterLocomotionData {
        Uuid idle;
        Uuid walkFwd;
        Uuid walkBack;
        Uuid walkLeft;
        Uuid walkRight;
        Uuid run;
        Uuid jump;
    };

    class AnimStateMachine;

    class ICharacterMoveController {
    public:
        ICharacterMoveController() {}
        virtual ~ICharacterMoveController() {}

        virtual void Tick(float deltaTime) = 0;

        FORCEINLINE float GetMoveSpeed() const { return moveSpeed; }

    protected:
        float moveSpeed = 0.f;
        float rotateSpeed = 0.f;
    };

    class CharacterLocomotion
        : public ComponentAdaptor<CharacterLocomotionData>
        , public ITransformEvent
        , public IAssetReadyNotifier {
    public:
        CharacterLocomotion() = default;
        ~CharacterLocomotion() override = default;

        COMPONENT_RUNTIME_INFO(CharacterLocomotion)

        static void Reflect(SerializationContext *context);

        void SetIdle(const Uuid& uuid);
        void SetWalkFwd(const Uuid& uuid);
        void SetWalkBack(const Uuid& uuid);
        void SetWalkLeft(const Uuid& uuid);
        void SetWalkRight(const Uuid& uuid);
        void SetRun(const Uuid& uuid);
        void SetJump(const Uuid& uuid);

        FORCEINLINE const Uuid& GetIdle() const { return data.idle; }
        FORCEINLINE const Uuid& GetWalkFwd() const { return data.walkFwd; }
        FORCEINLINE const Uuid& GetWalkBack() const { return data.walkBack; }
        FORCEINLINE const Uuid& GetWalkLeft() const { return data.walkLeft; }
        FORCEINLINE const Uuid& GetWalkRight() const { return data.walkRight; }
        FORCEINLINE const Uuid& GetRun() const { return data.run; }
        FORCEINLINE const Uuid& GetJump() const { return data.jump; }

    private:
        void OnTransformChanged(const Transform& global, const Transform& local);

        void OnAttachToWorld() override;
        void OnDetachFromWorld() override;

        void OnSerialized() override;

        void OnAssetLoaded(const Uuid& uuid) override;

        void Tick(float time) override;

        void UpdateAnimation();

        static constexpr uint32_t FullMask = (1 << static_cast<uint32_t>(CharacterStandardAction::NUM)) - 1;

        SingleAssetHolder<AnimationClip> holders[static_cast<size_t>(CharacterStandardAction::NUM)];
        uint32_t readyMask = 0;

        AnimStateMachine *stateMachine = nullptr;
        CounterPtr<SkeletonAnimation> animation;

        std::unique_ptr<ICharacterMoveController> controller;;
        std::unique_ptr<SkeletonDebugRender> debugRender;

        EventBinder<ITransformEvent> transformEvent;

        // transient status data
        Transform cachedTransform;
        AnimFinalPose cachedPose;
    };
} // namespace sky