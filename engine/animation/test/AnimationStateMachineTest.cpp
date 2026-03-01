//
// Created by blues on 2025/12/30.
//

#include <gtest/gtest.h>
#include <animation/graph/AnimationState.h>

using namespace sky;

TEST(AnimationTest, AnimationConditionTest)
{
    TAnimFuncParameter<float> p1([](float deltaTime) {
        return deltaTime;
    });

    std::unique_ptr<TAnimParameterCond<float>> cond = std::make_unique<TAnimParameterCond<float>>(
        &p1, 0.5f, AnimComp::GT
    );

    cond->Update(0.0f);
    ASSERT_FALSE(cond->Eval());

    cond->Update(0.5f);
    ASSERT_TRUE(!cond->Eval());

    cond->Update(0.51f);
    ASSERT_TRUE(cond->Eval());
}

TEST(AnimationTest, AnimationStateMachineTest)
{
    AnimStateMachine stateMachine;

    TAnimFuncParameter<float> pSpeed([](float delta) {
        return delta;
    });

    AnimHandle idle  = stateMachine.AddState(AnimState{Name("Idle")});
    AnimHandle walk  = stateMachine.AddState(AnimState{Name("walk")});
    AnimHandle run   = stateMachine.AddState(AnimState{Name("run")});

    AnimHandle startWalk = stateMachine.AddCondition(new TAnimParameterCond<float>(&pSpeed, 0.01f, AnimComp::GE));
    AnimHandle StopWalk = stateMachine.AddCondition(new TAnimParameterCond<float>(&pSpeed, 0.01f, AnimComp::LT));
    AnimHandle startRun = stateMachine.AddCondition(new TAnimParameterCond<float>(&pSpeed, 0.5f, AnimComp::GE));
    AnimHandle stopRun = stateMachine.AddCondition(new TAnimParameterCond<float>(&pSpeed, 0.5f, AnimComp::LT));

    stateMachine.AddTransition(AnimTransition{idle, walk, startWalk});
    stateMachine.AddTransition(AnimTransition{walk, idle, StopWalk});
    stateMachine.AddTransition(AnimTransition{walk, run, startRun});
    stateMachine.AddTransition(AnimTransition{run, walk, stopRun});
    stateMachine.Finalize();

    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), idle);

    pSpeed.Update(0.2f);
    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), walk);

    pSpeed.Update(0.4f);
    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), walk);

    pSpeed.Update(0.001f);
    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), idle);

    pSpeed.Update(0.6f);
    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), walk);

    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), run);

    pSpeed.Update(0.3f);
    stateMachine.TickAny({}, 0.2f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), walk);
}