//
// Created by Zach Lee on 2025/6/1.
//

#include <animation/graph/AnimationState.h>

namespace sky {

    void AnimStateMachine::AddState(const AnimState &state)
    {
        states.emplace_back(state);
    }

    void AnimStateMachine::AddTransition(const AnimTransition& transition)
    {
        transitions.emplace_back(transition);
    }

    void AnimStateMachine::Tick(AnimContext& context, float deltaTime)
    {
        if (currentState == ANIM_INVALID_HANDLE) {
            return;
        }
    }

    void AnimStateMachine::FindTransition(AnimContext& context)
    {


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
