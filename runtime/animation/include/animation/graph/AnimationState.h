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
#include <set>

namespace sky {

    using AnimHandle = uint32_t;
    static constexpr AnimHandle ANIM_INVALID_HANDLE = ~(0U);
    static constexpr uint32_t ANIM_MAX_TRANSITION_PER_FRAME = 1;

    struct AnimTransition {
        AnimHandle prevState = ANIM_INVALID_HANDLE;
        AnimHandle nextState = ANIM_INVALID_HANDLE;

        AnimHandle condition = ANIM_INVALID_HANDLE;
    };

    struct AnimState {
        Name name;
        AnimNode* node = nullptr;

        std::vector<AnimHandle> transitions;
    };

    class AnimStateMachine;
    using AnimStateMachinePtr = CounterPtr<AnimStateMachine>;

    class AnimStateMachine : public AnimNode {
    public:
        AnimStateMachine() = default;
        ~AnimStateMachine() override = default;

        // builder begin
        AnimStateMachine& AddState(const AnimState &state);
        AnimStateMachine& AddCondition(IAnimTransCond* cond);
        AnimStateMachine& AddTransition(const AnimTransition& transition);
        AnimStateMachine& SetEntry(AnimHandle initState);
        void Finalize();
        // builder end

        void SetState(const AnimContext& context, AnimHandle state);

    private:
        void InitAny(const AnimContext& context) override;
        void TickAny(const AnimLayerContext& context, float deltaTime) override;
        bool FindTransition(const AnimLayerContext& context, AnimHandle inState, AnimHandle& outTransition, std::set<AnimHandle>& visited);
        void Transition(const AnimLayerContext& context, AnimHandle trans);

        AnimHandle initState = ANIM_INVALID_HANDLE;
        AnimHandle currentState = ANIM_INVALID_HANDLE;
        float stateTime = 0.f;

        std::vector<AnimState> states;
        std::vector<AnimTransition> transitions;
        std::vector<std::unique_ptr<IAnimTransCond>> conditions;
    };

} // namespace sky
