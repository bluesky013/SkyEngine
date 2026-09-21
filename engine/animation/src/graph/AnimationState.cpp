//
// Created by Zach Lee on 2025/6/1.
//

#include <animation/graph/AnimationState.h>
#include <animation/plan/AnimationPlan.h>
#include <core/logger/Logger.h>
#include <algorithm>

static const char* TAG = "Animation";

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
        if (transition.prevState >= states.size() || transition.nextState >= states.size()) {
            LOG_E(TAG, "Invalid transition state handle prev[%u], next[%u]", transition.prevState, transition.nextState);
            return ANIM_INVALID_HANDLE;
        }

        if (transition.condition != ANIM_INVALID_HANDLE && transition.condition >= conditions.size()) {
            LOG_E(TAG, "Invalid transition condition handle [%u]", transition.condition);
            return ANIM_INVALID_HANDLE;
        }

        transitions.emplace_back(transition);

        auto transitionHandle = Cast<AnimHandle>(transitions.size() - 1);
        states[transition.prevState].transitions.emplace_back(transitionHandle);
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

    void AnimStateMachine::PreTick(const AnimationTick& tick)
    {

    }

    void AnimStateMachine::InitAny(const AnimContext& context)
    {
        stateTime = 0.f;
        currentState = ANIM_INVALID_HANDLE;
        SetState(context, initState);
    }

    void AnimStateMachine::UpdateControl(const AnimLayerContext& context, float deltaTime)
    {
        if (currentState == ANIM_INVALID_HANDLE || states.empty()) {
            return;
        }

        for (uint32_t index = 0; index < ANIM_MAX_TRANSITION_PER_FRAME; ++index) {
            AnimHandle validTransition = ANIM_INVALID_HANDLE;
            if (!FindTransition(context, currentState, validTransition)) {
                break;
            }
            Transition(context, validTransition);
        }
    }

    void AnimStateMachine::TickAny(const AnimLayerContext& context, float deltaTime)
    {
        UpdateControl(context, deltaTime);

        if (currentState < states.size() && states[currentState].node != nullptr) {
            states[currentState].node->TickAny(context, deltaTime);
        }
    }

    void AnimStateMachine::EvalAny(AnimationEval& context)
    {
        if (currentState >= states.size() || states[currentState].node == nullptr) {
            return;
        }

        states[currentState].node->EvalAny(context);
    }

    void AnimStateMachine::Transition(const AnimLayerContext& context, AnimHandle trans)
    {
        if (trans >= transitions.size()) {
            return;
        }

        const auto& transition = transitions[trans];
        SetState(context, transition.nextState);
    }

    bool AnimStateMachine::FindTransition(const AnimLayerContext& context, AnimHandle inState, AnimHandle& outTransition)
    {
        if (inState >= states.size()) {
            return false;
        }

        const auto& stateInfo = states[inState];
        for (const auto &transHandle : stateInfo.transitions) {
            if (transHandle >= transitions.size()) {
                continue;
            }

            const auto& transition = transitions[transHandle];
            const IAnimTransCond* condition = (transition.condition != ANIM_INVALID_HANDLE && transition.condition < conditions.size())
                ? conditions[transition.condition].get()
                : nullptr;

            if (condition != nullptr && condition->Eval()) {
                outTransition = transHandle;
                return true;
            }
        }
        return false;
    }

    void AnimStateMachine::SetState(const AnimContext& context, AnimHandle state)
    {
        if (states.empty() || state == ANIM_INVALID_HANDLE) {
            return;
        }

        const AnimHandle clamped = std::min(state, static_cast<AnimHandle>(states.size() - 1));
        if (currentState == clamped) {
            return;
        }

        stateTime = 0.f;
        currentState = clamped;

        auto& animState = states[currentState];
        if (animState.node != nullptr) {
            animState.node->InitAny(context);
        }
    }

    bool AnimStateMachine::LowerToPlan(AnimationPlanBuilder& builder, const AnimPlanLowerInfo& info, uint32_t& outSlot)
    {
        const uint32_t numStates = GetNumStates();
        if (numStates == 0) {
            builder.AddError("state machine has no states");
            return false;
        }

        AnimationStateBinding binding;
        binding.node = this;
        binding.stateOpRanges.reserve(numStates);

        std::vector<uint16_t> stateSlots;
        stateSlots.reserve(numStates);

        for (uint32_t i = 0; i < numStates; ++i) {
            const AnimState& state = states[i];
            if (state.node == nullptr) {
                builder.AddError("state has no node");
                return false;
            }

            const uint32_t firstOp = builder.GetOpCount();
            uint32_t stateSlot = ANIM_INVALID_SLOT;
            if (!state.node->LowerToPlan(builder, info, stateSlot)) {
                return false;
            }

            const uint32_t opCount = builder.GetOpCount();
            const uint32_t lastOp = opCount > firstOp ? opCount - 1 : firstOp;

            stateSlots.push_back(static_cast<uint16_t>(stateSlot));
            binding.stateOpRanges.push_back(AnimStateOpRange{firstOp, lastOp});
        }

        const uint32_t tableBase = builder.AddStateTable(stateSlots);
        const uint32_t slot = builder.AllocateSlot();

        AnimOpRecord op;
        op.op = AnimOp::StateSelect;
        op.output = static_cast<uint16_t>(slot);
        op.dataIndex = tableBase;
        builder.EmitOp(op);

        builder.AddStateBinding(binding);
        outSlot = slot;
        return true;
    }

} // namespace sky
