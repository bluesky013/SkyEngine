//
// Created by Zach Lee on 2025/6/1.
//

#pragma once

#include <core/name/Name.h>
#include <animation/core/AnimationTypes.h>
#include <animation/core/AnimationCondition.h>
#include <animation/graph/AnimationNode.h>
#include <functional>
#include <memory>

namespace sky {

    using AnimHandle = uint32_t;
    static constexpr AnimHandle ANIM_INVALID_HANDLE = ~(0U);

    struct AnimState {
        Name name;
        AnimHandle outState = ANIM_INVALID_HANDLE;
    };

    struct AnimTransition {
        float crossFade = 0.f;
        AnimHandle prevState = ANIM_INVALID_HANDLE;
        AnimHandle nextState = ANIM_INVALID_HANDLE;

        AnimHandle condition = ANIM_INVALID_HANDLE;
    };

    class AnimStateMachine;
    using AnimStateMachinePtr = CounterPtr<AnimStateMachine>;

    class AnimStateMachine : public AnimNode {
    public:
        AnimStateMachine() = default;
        ~AnimStateMachine() override = default;

        void AddState(const AnimState &state);
        void AddTransition(const AnimTransition& transition);
    private:
        void Tick(AnimContext& context, float deltaTime) override;
        void FindTransition(AnimContext& context);

        AnimHandle currentState = ANIM_INVALID_HANDLE;

        std::vector<AnimState> states;
        std::vector<AnimTransition> transitions;
        std::vector<std::unique_ptr<IAnimTransCond>> conditions;
    };

} // namespace sky
