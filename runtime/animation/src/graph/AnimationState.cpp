//
// Created by Zach Lee on 2025/6/1.
//

#include <animation/graph/AnimationState.h>
#include <algorithm>

namespace sky {

    AnimStateMachine& AnimStateMachine::AddState(const AnimState &state)
    {
        states.emplace_back(state);
        return *this;
    }

    AnimStateMachine& AnimStateMachine::AddCondition(IAnimTransCond* cond)
    {
        conditions.emplace_back(cond);
        return *this;
    }

    AnimStateMachine& AnimStateMachine::AddTransition(const AnimTransition& transition)
    {
        transitions.emplace_back(transition);
        return *this;
    }

    AnimStateMachine& AnimStateMachine::SetEntry(AnimHandle inState)
    {
        initState = inState;
        return *this;
    }

    void AnimStateMachine::Finalize()
    {
        if (initState == ANIM_INVALID_HANDLE && !states.empty()) {
            initState = 0;
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

    void AnimStateMachine::Transition(const AnimLayerContext& context, AnimHandle trans)
    {

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

            const AnimState& nextState = states[transition.nextState];
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
            states[currentState].node->InitAny(context);
        }
    }

//    void AnimStateMachine::Update(float deltaTime)
//    {
//        if (!currentState) {
//            return;
//        }
//
//        currentState->onUpdate(deltaTime);
//
//        for (const auto& transition : currentState->transitions) {
//            if (transition.condition()) {
//                ChangeState(transition.target);
//                break;
//            }
//        }
//    }

} // namespace sky
