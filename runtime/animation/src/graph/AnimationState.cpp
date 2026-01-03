//
// Created by Zach Lee on 2025/6/1.
//

#include <animation/graph/AnimationState.h>
#include <algorithm>

namespace sky {

    AnimHandle AnimStateMachine::AddState(const AnimState &state)
    {
        states.emplace_back(state);
        return Cast<AnimHandle>(states.size() - 1);
    }

    AnimHandle AnimStateMachine::AddCondition(IAnimTransCond* cond)
    {
        conditions.emplace_back(cond);
        return Cast<AnimHandle>(conditions.size() - 1);
    }

    AnimHandle AnimStateMachine::AddTransition(const AnimTransition& transition)
    {
        transitions.emplace_back(transition);

        SKY_ASSERT(transition.prevState != ANIM_INVALID_HANDLE);
        SKY_ASSERT(transition.nextState != ANIM_INVALID_HANDLE);
        SKY_ASSERT(transition.condition != ANIM_INVALID_HANDLE);

        auto transitionHandle = Cast<AnimHandle>(transitions.size() - 1);

        auto& prevState = states[transition.prevState];
        prevState.transitions.emplace_back(transitionHandle);
        return transitionHandle;
    }

    void AnimStateMachine::SetEntry(AnimHandle inState)
    {
        initState = inState;
    }

    void AnimStateMachine::Finalize()
    {
        if (initState == ANIM_INVALID_HANDLE && !states.empty()) {
            initState = 0;
            currentState = initState;
        }
    }

    void AnimStateMachine::InitAny(const AnimContext& context)
    {
        stateTime = 0.f;
        currentState = ANIM_INVALID_HANDLE;
    }

    void AnimStateMachine::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        if (currentState == ANIM_INVALID_HANDLE || states.empty()) {
            return;
        }

        AnimHandle validTransition = ANIM_INVALID_HANDLE;

        std::set<AnimHandle> visited;
        if (FindTransition(context, currentState, validTransition, visited) && (validTransition < transitions.size())) {
            Transition(context, validTransition);
        }
    }

    void AnimStateMachine::EvalAny(AnimationEval& context)
    {

    }

    void AnimStateMachine::Transition(const AnimLayerContext& context, AnimHandle trans)
    {
        const auto& transition = transitions[trans];
        SetState(context, transition.nextState);
    }

    bool AnimStateMachine::FindTransition(const AnimLayerContext& context, AnimHandle inState, AnimHandle& outTransition, std::set<AnimHandle>& visited)
    {
        if (visited.contains(inState)) {
            return false;
        }

        visited.emplace(inState);

        const auto& stateInfo = states[inState];
        for (const auto &transHandle : stateInfo.transitions) {
            const auto& transition = transitions[transHandle];
            const IAnimTransCond* condition = transition.condition != ANIM_INVALID_HANDLE ? conditions[transition.condition].get() : nullptr;
            bool enterNextState = (condition != nullptr) && (condition->Eval());
            if (enterNextState) {
                outTransition = transHandle;
                return true;
            }
        }
        return false;
    }

    void AnimStateMachine::SetState(const AnimContext& context, AnimHandle state)
    {
        if (currentState != state && !states.empty()) {

            stateTime = 0.f;
            currentState = std::clamp<AnimHandle>(state, 0, static_cast<AnimHandle>(states.size()));

            // initialize node
            auto& animState = states[currentState];
            if (animState.node != nullptr) {
                states[currentState].node->InitAny(context);
            }
        }
    }

} // namespace sky
