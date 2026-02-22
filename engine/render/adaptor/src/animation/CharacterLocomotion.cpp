//
// Created by Zach Lee on 2026/1/4.
//

#include "framework/window/IWindowEvent.h"
#include "render/adaptor/event/PoseUpdateEvent.h"

#include <render/adaptor/RenderSceneProxy.h>
#include <render/adaptor/animation/CharacterLocomotion.h>
#include <render/adaptor/assets/AnimationAsset.h>
#include <render/adaptor/assets/SkeletonAsset.h>

#include <framework/serialization/SerializationContext.h>
#include <framework/world/TransformComponent.h>
#include <render/RenderTechniqueLibrary.h>

#include <core/util/Cast.h>

#include <animation/graph/AnimationState.h>
#include <animation/graph/AnimationClipNode.h>

namespace sky {

    /**
     * Temp Character Controller
     */
    class CharacterTrdViewController
        : public ICharacterMoveController
        , public IKeyboardEvent
    {
    public:
        explicit CharacterTrdViewController(TransformComponent* inTrans) : transform(inTrans)
        {
            keyBinder.Bind(this);
        }

        static constexpr float ROTATE_SPEED = 100.f;
        static constexpr float MOVE_SPEED = 200.f;

        void Tick(float deltaTime) override
        {
            Transform localTrans = transform->GetLocalTransform();
            auto euler = localTrans.rotation.ToEulerYZX();

            bool leftDown = keyDown[ToSize(ScanCode::KEY_LEFT)];
            bool rightDown = keyDown[ToSize(ScanCode::KEY_RIGHT)];
            bool upDown = keyDown[ToSize(ScanCode::KEY_UP)];
            bool backDown = keyDown[ToSize(ScanCode::KEY_DOWN)];

            bool isTurn = leftDown ^ rightDown;

            rotateSpeed = 0.f;
            if (isTurn) {
                rotateSpeed = leftDown ? ROTATE_SPEED : -ROTATE_SPEED;
                euler.y += rotateSpeed * deltaTime;
                localTrans.rotation.FromEulerYZX(euler);
            }


            bool isMove = upDown ^ backDown;
            moveSpeed = 0.f;
            if (isMove) {
                auto forward = localTrans.rotation * (VEC3_Z);
                moveSpeed = upDown ? MOVE_SPEED : -MOVE_SPEED;
                if (mod.TestBit(KeyMod::SHIFT)) {
                    moveSpeed *= 2.f;
                }

                localTrans.translation += forward * deltaTime * moveSpeed;
            }

            transform->SetLocalTransform(localTrans);
        }

    private:
        void OnKeyUp(const KeyboardEvent &event) override
        {
            auto& key = keyDown[ToSize(event.scanCode)];
            key = false;
            mod = event.mod;
        }

        void OnKeyDown(const KeyboardEvent &event) override
        {
            auto& key = keyDown[ToSize(event.scanCode)];
            key = true;
            mod = event.mod;
        }

        bool keyDown[static_cast<uint32_t>(ScanCode::KEY_NUM)] = {false};
        KeyModFlags mod;

        TransformComponent* transform;
        EventBinder<IKeyboardEvent> keyBinder;
    };


    void CharacterLocomotion::Reflect(SerializationContext *context)
    {
        context->Register<CharacterLocomotionData>("CharacterLocomotionData")
            .Member<&CharacterLocomotionData::idle>("Idle")
            .Member<&CharacterLocomotionData::walkFwd>("WalkFwd")
            .Member<&CharacterLocomotionData::walkBack>("WalkBack")
            .Member<&CharacterLocomotionData::walkLeft>("WalkLeft")
            .Member<&CharacterLocomotionData::walkRight>("WalkRight")
            .Member<&CharacterLocomotionData::run>("Run")
            .Member<&CharacterLocomotionData::jump>("Jump");

        REGISTER_BEGIN(CharacterLocomotion, context)
            REGISTER_MEMBER(Idle, SetIdle, GetIdle)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(WalkFwd, SetWalkFwd, GetWalkFwd)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(WalkBack, SetWalkBack, GetWalkBack)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(WalkLeft, SetWalkLeft, GetWalkLeft)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(WalkRight, SetWalkRight, GetWalkRight)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(Run, SetRun, GetRun)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE)
            REGISTER_MEMBER(Jump, SetJump, GetJump)
                SET_ASSET_TYPE(AssetTraits<AnimationClip>::ASSET_TYPE);
    }

    void CharacterLocomotion::OnSerialized()
    {
        SetIdle(data.idle);
        SetWalkFwd(data.walkFwd);
        SetWalkBack(data.walkBack);
        SetWalkLeft(data.walkLeft);
        SetWalkRight(data.walkRight);
        SetRun(data.run);
        SetJump(data.jump);
    }

    void CharacterLocomotion::SetIdle(const Uuid& uuid)
    {
        data.idle = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::IDLE));
        holders[ToSize(CharacterStandardAction::IDLE)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetWalkFwd(const Uuid& uuid)
    {
        data.walkFwd = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::WALK_FWD));
        holders[ToSize(CharacterStandardAction::WALK_FWD)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetWalkBack(const Uuid& uuid)
    {
        data.walkBack = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::WALK_BACK));
        holders[ToSize(CharacterStandardAction::WALK_BACK)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetWalkLeft(const Uuid& uuid)
    {
        data.walkLeft = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::WALK_LEFT));
        holders[ToSize(CharacterStandardAction::WALK_LEFT)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetWalkRight(const Uuid& uuid)
    {
        data.walkRight = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::WALK_RIGHT));
        holders[ToSize(CharacterStandardAction::WALK_RIGHT)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetRun(const Uuid& uuid)
    {
        data.run = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::RUN));
        holders[ToSize(CharacterStandardAction::RUN)].SetAsset(uuid, this);
    }
    void CharacterLocomotion::SetJump(const Uuid& uuid)
    {
        data.jump = uuid;
        readyMask &=~ (1 << ToSize(CharacterStandardAction::JUMP));
        holders[ToSize(CharacterStandardAction::JUMP)].SetAsset(uuid, this);
    }

    void CharacterLocomotion::OnAttachToWorld()
    {
        if (!debugRender) {
            debugRender = std::make_unique<SkeletonDebugRender>();
            debugRender->SetTechnique(RenderTechniqueLibrary::Get()->FetchGfxTechnique(Name("techniques/debug.tech")));
        }

        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        // renderScene->AddPrimitive(debugRender->GetPrimitive());

        transformEvent.Bind(this, actor);
        cachedTransform = actor->GetComponent<TransformComponent>()->GetWorldTransform();
    }

    void CharacterLocomotion::OnDetachFromWorld()
    {
        auto *renderScene = static_cast<RenderSceneProxy*>(actor->GetWorld()->GetSubSystem(Name("RenderScene")))->GetRenderScene();
        // renderScene->RemovePrimitive(debugRender->GetPrimitive());

        transformEvent.Reset();
    }

    void CharacterLocomotion::OnTransformChanged(const Transform& global, const Transform& local)
    {
        cachedTransform = global;
    }

    void CharacterLocomotion::OnAssetLoaded(const Uuid& uuid, const std::string_view&)
    {
        uint32_t lastMask = readyMask;
        for (uint32_t i = 0; i < ToSize(CharacterStandardAction::NUM); i++) {
            if (holders[i].GetUuid() == uuid) {
                readyMask |= (1 << i);
            }
        }
        if (lastMask != FullMask && readyMask == FullMask) {
            UpdateAnimation();
        }
    }

    void CharacterLocomotion::Tick(float time)
    {
        if (!animation) {
            return;
        }

        if (controller != nullptr) {
            controller->Tick(time);
        }

        AnimationTick tick = {time};
        animation->Tick(tick);

        AnimationEval eval(animation->GetSkeleton());
        animation->EvalAny(eval);
        // debugRender->DrawPose(eval.pose, cachedTransform);
        Event<IPoseUpdateEvent>::BroadCast(actor, &IPoseUpdateEvent::OnPoseUpdated, AnimFinalPose(eval.pose));

        // debugRender->DrawPose(*animation->GetSkeleton()->GetRefPos(), cachedTransform);
        // Event<IPoseUpdateEvent>::BroadCast(actor, &IPoseUpdateEvent::OnPoseUpdated, AnimFinalPose(*animation->GetSkeleton()->GetRefPos()));
    }

    void CharacterLocomotion::UpdateAnimation()
    {
        if (controller == nullptr) {
            controller = std::make_unique<CharacterTrdViewController>(actor->GetComponent<TransformComponent>());
        }

        static Name Names[] = {
            Name("IDLE"),
            Name("WALK_FWD"),
            Name("WALK_BACK"),
            Name("WALK_LEFT"),
            Name("WALK_RIGHT"),
            Name("RUN"),
            Name("JUMP")
        };

        // [loop, root motion, entry]
        static std::tuple<bool, bool, bool> ClipParam[] = {
            {true, false, true},
            {true, false, false},
            {true, false, false},
            {true, false, false},
            {true, false, false},
            {true, false, false},
            {true, false, false},
            {false, false, false},
        };


        animation = new SkeletonAnimation();
        auto skeletonAsset = AssetManager::Get()->FindAsset<Skeleton>(holders[0].Data().skeleton);
        SkeletonPtr skl = FetchOrCreateSkeletonByAsset(skeletonAsset);
        cachedPose.SetSkeleton(skl);
        std::array<AnimHandle, static_cast<size_t>(CharacterStandardAction::NUM)> stateHandles;

        stateMachine = animation->NewAnimNode<AnimStateMachine>();
        for (uint32_t i = 0; i < ToSize(CharacterStandardAction::NUM); i++) {
            AnimationClipNode::PersistentData initData = {};
            initData.clip = CreateAnimationClipFromAsset(holders[i].GetAsset());

            bool entryNode = false;
            std::tie(initData.looping, initData.rootMotion, entryNode) = ClipParam[i];

            AnimState animState = {};
            animState.name = Names[i];
            animState.node = animation->NewAnimNode<AnimationClipNode>(initData);

            auto handle = stateMachine->AddState(animState);
            if (entryNode) {
                stateMachine->SetEntry(handle);
            }

            stateHandles[i] = handle;
        }

        auto *getSpeed = animation->NewParameter<TAnimFuncParameter<float>>(Name("MoveSpeed"), [this](float deltaTime) {
            return std::abs(controller->GetMoveSpeed());
        });

        auto toWalk = stateMachine->AddCondition(new TAnimParameterCond(getSpeed, 0.5f, AnimComp::GT));
        auto toRun = stateMachine->AddCondition(new TAnimParameterCond(getSpeed, CharacterTrdViewController::MOVE_SPEED, AnimComp::GT));
        auto runToWalk = stateMachine->AddCondition(new TAnimParameterCond(getSpeed, CharacterTrdViewController::MOVE_SPEED, AnimComp::LE));
        auto toIdle = stateMachine->AddCondition(new TAnimParameterCond(getSpeed, 0.5f, AnimComp::LE));

        stateMachine->AddTransition(AnimTransition(stateHandles[ToSize(CharacterStandardAction::IDLE)], stateHandles[ToSize(CharacterStandardAction::WALK_FWD)], toWalk));
        stateMachine->AddTransition(AnimTransition(stateHandles[ToSize(CharacterStandardAction::IDLE)], stateHandles[ToSize(CharacterStandardAction::RUN)], toRun));

        stateMachine->AddTransition(AnimTransition(stateHandles[ToSize(CharacterStandardAction::WALK_FWD)], stateHandles[ToSize(CharacterStandardAction::RUN)], toRun));
        stateMachine->AddTransition(AnimTransition(stateHandles[ToSize(CharacterStandardAction::WALK_FWD)], stateHandles[ToSize(CharacterStandardAction::IDLE)], toIdle));

        stateMachine->AddTransition(AnimTransition(stateHandles[ToSize(CharacterStandardAction::RUN)], stateHandles[ToSize(CharacterStandardAction::WALK_FWD)], runToWalk));

        stateMachine->Finalize();

        SkeletonAnimationInit animInit = {};
        animInit.skeleton = skl;
        animInit.rootNode = stateMachine;
        animation->Init(animInit);
    }

} // namespace sky