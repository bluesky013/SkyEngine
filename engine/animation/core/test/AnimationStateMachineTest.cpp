//
// Created by blues on 2025/12/30.
//

#include <gtest/gtest.h>
#include <animation/graph/AnimationState.h>

using namespace sky;

namespace {

    class StateInitCounter : public AnimNode {
    public:
        void InitAny(const AnimContext&) override { ++initCount; }
        void EvalAny(AnimationEval&) override {}

        int initCount = 0;
    };

} // namespace

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

TEST(AnimationTest, AnimationStateMachineDefaultEntryTest)
{
    AnimStateMachine stateMachine;
    stateMachine.AddState(AnimState{Name("A")});
    stateMachine.AddState(AnimState{Name("B")});
    stateMachine.Finalize();

    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), 0u);
}

TEST(AnimationTest, AnimationStateMachineFalseConditionTest)
{
    AnimStateMachine stateMachine;

    TAnimFuncParameter<float> speed([](float) { return 0.f; });

    AnimHandle a = stateMachine.AddState(AnimState{Name("A")});
    AnimHandle b = stateMachine.AddState(AnimState{Name("B")});
    AnimHandle cond = stateMachine.AddCondition(new TAnimParameterCond<float>(&speed, 0.5f, AnimComp::GE));

    stateMachine.AddTransition(AnimTransition{a, b, cond});
    stateMachine.Finalize();

    speed.Update(0.f);
    stateMachine.TickAny({}, 0.1f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), a);
}

TEST(AnimationTest, AnimationStateMachineSingleTransitionTest)
{
    AnimStateMachine stateMachine;

    TAnimFuncParameter<float> speed([](float) { return 1.f; });

    AnimHandle a = stateMachine.AddState(AnimState{Name("A")});
    AnimHandle b = stateMachine.AddState(AnimState{Name("B")});
    AnimHandle c = stateMachine.AddState(AnimState{Name("C")});

    AnimHandle condAB = stateMachine.AddCondition(new TAnimParameterCond<float>(&speed, 0.5f, AnimComp::GE));
    AnimHandle condAC = stateMachine.AddCondition(new TAnimParameterCond<float>(&speed, 0.1f, AnimComp::GE));

    stateMachine.AddTransition(AnimTransition{a, b, condAB});
    stateMachine.AddTransition(AnimTransition{a, c, condAC});
    stateMachine.Finalize();

    speed.Update(1.f);
    stateMachine.TickAny({}, 0.1f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), b);
}

TEST(AnimationTest, AnimationStateMachineNonCurrentTransitionTest)
{
    AnimStateMachine stateMachine;

    TAnimFuncParameter<float> always([](float) { return 1.f; });

    AnimHandle a = stateMachine.AddState(AnimState{Name("A")});
    AnimHandle b = stateMachine.AddState(AnimState{Name("B")});
    AnimHandle c = stateMachine.AddState(AnimState{Name("C")});
    AnimHandle cond = stateMachine.AddCondition(new TAnimParameterCond<float>(&always, 0.f, AnimComp::AWS));

    stateMachine.AddTransition(AnimTransition{b, c, cond});
    stateMachine.Finalize();

    always.Update(1.f);
    stateMachine.TickAny({}, 0.1f);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), a);
}

TEST(AnimationTest, AnimationStateMachineReentryTest)
{
    AnimStateMachine stateMachine;

    StateInitCounter counter;
    AnimState stateA;
    stateA.name = Name("A");
    stateA.node = &counter;

    TAnimFuncParameter<float> always([](float) { return 1.f; });

    AnimHandle a = stateMachine.AddState(stateA);
    AnimHandle cond = stateMachine.AddCondition(new TAnimParameterCond<float>(&always, 0.f, AnimComp::AWS));
    stateMachine.AddTransition(AnimTransition{a, a, cond});
    stateMachine.Finalize();

    AnimContext context{};
    stateMachine.InitAny(context);
    ASSERT_EQ(counter.initCount, 1);

    always.Update(1.f);
    stateMachine.TickAny({}, 0.1f);
    ASSERT_EQ(counter.initCount, 1);
}

TEST(AnimationTest, AnimationStateMachineBoundsTest)
{
    AnimStateMachine stateMachine;
    stateMachine.AddState(AnimState{Name("A")});
    stateMachine.AddState(AnimState{Name("B")});
    stateMachine.AddState(AnimState{Name("C")});
    stateMachine.Finalize();

    AnimHandle invalid = stateMachine.AddTransition(AnimTransition{99u, 0u, ANIM_INVALID_HANDLE});
    ASSERT_EQ(invalid, ANIM_INVALID_HANDLE);

    stateMachine.SetEntry(99u);
    AnimContext context{};
    stateMachine.InitAny(context);
    ASSERT_EQ(stateMachine.GetCurrentStateHandle(), 2u);
}