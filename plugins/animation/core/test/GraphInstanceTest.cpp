#include <algorithm>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/graph/GraphInstance.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::animation;

namespace {
    AnimClipPtr Constant(float x, uint32_t frames = 4)
    {
        AnimClipPtr clip = new AnimationClip(Name("constant"));
        clip->SetFrameRate(1.f);
        clip->SetNumFrame(frames);
        AnimNodeChannelData channel;
        channel.name           = "root";
        channel.position.times = {0};
        channel.position.keys  = {Vector3(x, 0.f, 0.f)};
        EXPECT_TRUE(clip->AddChannel(new AnimationNodeChannel(channel)));
        return clip;
    }
    struct Fixture {
        SkeletonPtr          rig;
        GraphDefinition      graph;
        GraphInstance::Clips clips;
        Uuid                 left = Uuid::Create(), right = Uuid::Create();
        Uuid                 a = Uuid::Create(), b = Uuid::Create();
        Uuid                 leftNode = Uuid::Create(), rightNode = Uuid::Create();
        Uuid                 moving = Uuid::Create(), weight = Uuid::Create();
        Fixture()
        {
            SkeletonData data;
            data.boneData = {{Name("root"), INVALID_BONE_ID}, {Name("helper"), 0}};
            data.refPos.resize(2);
            data.refPos[1].translation = Vector3(0.f, 3.f, 0.f);
            rig                        = Skeleton::BuildSkeleton(data);
            graph.document             = Uuid::Create();
            graph.skeleton             = Uuid::Create();
            graph.parameters           = {{moving, "moving", GraphParameterType::Bool, false}, {weight, "weight", GraphParameterType::Float, 0.25f}};
            GraphNode idle;
            idle.id   = leftNode;
            idle.clip = left;
            GraphNode walk;
            walk.id   = rightNode;
            walk.clip = right;
            GraphNode outputA;
            outputA.id   = Uuid::Create();
            outputA.kind = GraphNodeKind::Output;
            GraphNode outputB;
            outputB.id       = Uuid::Create();
            outputB.kind     = GraphNodeKind::Output;
            graph.nodes      = {idle, outputA, walk, outputB};
            graph.links      = {{idle.id, GraphPin::Pose, outputA.id, GraphPin::InputA}, {walk.id, GraphPin::Pose, outputB.id, GraphPin::InputA}};
            graph.states     = {{a, "Idle", outputA.id}, {b, "Walk", outputB.id}};
            graph.entryState = a;
            GraphTransition transition;
            transition.id         = Uuid::Create();
            transition.from       = a;
            transition.to         = b;
            transition.conditions = {{moving, GraphComparison::Equal, true}};
            graph.transitions.push_back(transition);
            transition.id                  = Uuid::Create();
            transition.from                = b;
            transition.to                  = a;
            transition.conditions[0].value = false;
            graph.transitions.push_back(transition);
            clips.emplace(left, Constant(0.f));
            clips.emplace(right, Constant(8.f));
        }
        std::unique_ptr<GraphInstance> Create()
        {
            CookedGraph cooked;
            EXPECT_TRUE(CompileGraphDefinition(graph, cooked));
            AnimationStatus status;
            auto            instance = GraphInstance::Create(cooked, rig, clips, status);
            EXPECT_TRUE(status) << status.message;
            return instance;
        }
    };
    GraphNodeDebug Clock(const GraphInstance &instance, const Uuid &node)
    {
        const auto debug = instance.Debug();
        const auto found = std::find_if(debug.nodes.begin(), debug.nodes.end(), [&](const auto &clock) { return clock.node == node; });
        EXPECT_NE(found, debug.nodes.end());
        return found == debug.nodes.end() ? GraphNodeDebug{} : *found;
    }
} // namespace

TEST(GraphInstance, InstancesOwnParametersPlansAndInactiveClocks)
{
    Fixture f;
    auto    first  = f.Create();
    auto    second = f.Create();
    ASSERT_TRUE(first && second);
    first->SetPlaying(true);
    second->SetPlaying(true);
    EXPECT_TRUE(first->SetParameter(f.moving, true));
    ASSERT_TRUE(first->Tick(1.f));
    ASSERT_TRUE(second->Tick(0.5f));
    EXPECT_EQ(first->Debug().activeState, f.b);
    EXPECT_EQ(second->Debug().activeState, f.a);
    EXPECT_DOUBLE_EQ(Clock(*first, f.leftNode).elapsed, 0.0);
    EXPECT_DOUBLE_EQ(Clock(*first, f.rightNode).elapsed, 1.0);
    EXPECT_DOUBLE_EQ(Clock(*second, f.rightNode).elapsed, 0.0);
    AnimPose a, b;
    first->Evaluate(a);
    second->Evaluate(b);
    EXPECT_FLOAT_EQ(a.transforms[0].translation.x, 8.f);
    EXPECT_FLOAT_EQ(b.transforms[0].translation.x, 0.f);
    EXPECT_FLOAT_EQ(a.transforms[1].translation.y, 3.f);
    const auto *storage = a.transforms.data();
    for (uint32_t i = 0; i < 100; ++i) {
        ASSERT_TRUE(first->Tick(0.01f));
        first->Evaluate(a);
    }
    EXPECT_EQ(storage, a.transforms.data());
}

TEST(GraphInstance, ExitTimeUsesUnwrappedSourceClockAndChecksAtTickStart)
{
    Fixture f;
    f.graph.transitions[0].hasExitTime    = true;
    f.graph.transitions[0].timeSourceNode = f.leftNode;
    f.graph.transitions[0].exitTime       = 0.5f;
    auto instance                         = f.Create();
    ASSERT_TRUE(instance);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(2.f));
    EXPECT_EQ(instance->Debug().activeState, f.a);
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().activeState, f.b);
    instance->Stop();
    ASSERT_TRUE(instance->SetParameter(f.moving, false));
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->Tick(10.f));
    EXPECT_FLOAT_EQ(Clock(*instance, f.leftNode).sampleTime, 2.f);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().activeState, f.b);
}

TEST(GraphInstance, CrossfadeIsUninterruptibleAndLargeDeltaAdvancesEachSideCorrectly)
{
    Fixture f;
    f.graph.transitions[0].duration = 2.f;
    auto instance                   = f.Create();
    ASSERT_TRUE(instance);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(0.5f));
    auto debug = instance->Debug();
    EXPECT_EQ(debug.activeState, f.a);
    EXPECT_EQ(debug.destinationState, f.b);
    EXPECT_FLOAT_EQ(debug.progress, 0.25f);
    AnimPose pose;
    instance->Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 2.f);
    ASSERT_TRUE(instance->SetParameter(f.moving, false));
    ASSERT_TRUE(instance->Tick(4.f));
    EXPECT_EQ(instance->Debug().activeState, f.b);
    EXPECT_FALSE(instance->Debug().transition);
    EXPECT_DOUBLE_EQ(Clock(*instance, f.leftNode).elapsed, 2.0);
    EXPECT_DOUBLE_EQ(Clock(*instance, f.rightNode).elapsed, 4.5);
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().activeState, f.a);
    EXPECT_DOUBLE_EQ(Clock(*instance, f.leftNode).elapsed, 0.0);
}

TEST(GraphInstance, PauseRateZeroAndStopPreserveParameterContract)
{
    Fixture f;
    auto    instance = f.Create();
    ASSERT_TRUE(instance);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(10.f));
    EXPECT_EQ(instance->Debug().activeState, f.a);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetRate(0.f));
    ASSERT_TRUE(instance->Tick(10.f));
    EXPECT_EQ(instance->Debug().activeState, f.a);
    ASSERT_TRUE(instance->SetRate(2.f));
    ASSERT_TRUE(instance->Tick(0.5f));
    EXPECT_DOUBLE_EQ(Clock(*instance, f.rightNode).elapsed, 1.0);
    EXPECT_FALSE(instance->SetParameter(f.moving, 1.f));
    EXPECT_FALSE(instance->SetParameter(f.weight, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(instance->SetParameter(Uuid::Create(), true));
    EXPECT_FALSE(instance->SetRate(-1.f));
    EXPECT_FALSE(instance->Tick(-1.f));
    instance->Stop();
    EXPECT_EQ(instance->Debug().activeState, f.a);
    EXPECT_FALSE(instance->IsPlaying());
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().activeState, f.b);
}

TEST(GraphInstance, BlendParametersClampAndPartialChannelsAlwaysRefillReference)
{
    Fixture f;
    f.graph.states.clear();
    f.graph.transitions.clear();
    f.graph.entryState = {};
    const auto output  = f.graph.nodes[1];
    f.graph.nodes.erase(f.graph.nodes.begin() + 3);
    GraphNode blend;
    blend.id             = Uuid::Create();
    blend.kind           = GraphNodeKind::Blend2;
    blend.alphaParameter = f.weight;
    f.graph.nodes.push_back(blend);
    f.graph.output = output.id;
    f.graph.links  = {{f.leftNode, GraphPin::Pose, blend.id, GraphPin::InputA},
                      {f.rightNode, GraphPin::Pose, blend.id, GraphPin::InputB},
                      {blend.id, GraphPin::Pose, output.id, GraphPin::InputA}};
    auto instance  = f.Create();
    ASSERT_TRUE(instance);
    AnimPose pose;
    instance->Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 2.f);
    ASSERT_TRUE(instance->SetParameter(f.weight, 10.f));
    instance->Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 8.f);
    pose.transforms[1].translation = Vector3(99.f);
    ASSERT_TRUE(instance->SetParameter(f.weight, -10.f));
    instance->Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 0.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 3.f);
}

TEST(GraphInstance, PriorityAndStableIdBreakTiesWithoutChainedTransitions)
{
    Fixture f;
    auto    second  = f.graph.transitions[0];
    second.id       = Uuid::Create();
    second.priority = 2;
    second.duration = 1.f;
    f.graph.transitions.push_back(second);
    f.graph.transitions[0].priority = 1;
    auto instance                   = f.Create();
    ASSERT_TRUE(instance);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().transition, second.id);
    f.graph.transitions[0].priority = 2;
    f.graph.transitions[0].duration = 1.f;
    instance                        = f.Create();
    ASSERT_TRUE(instance);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(0.f));
    const auto expected = second.id.ToString() < f.graph.transitions[0].id.ToString() ? second.id : f.graph.transitions[0].id;
    EXPECT_EQ(instance->Debug().transition, expected);
}

TEST(GraphInstance, ZeroDurationExitClockOnlyAcceptsZeroThreshold)
{
    Fixture     f;
    AnimClipPtr empty = new AnimationClip(Name("empty"));
    empty->SetFrameRate(1.f);
    f.clips[f.left]                       = empty;
    f.graph.transitions[0].hasExitTime    = true;
    f.graph.transitions[0].timeSourceNode = f.leftNode;
    f.graph.transitions[0].exitTime       = 0.5f;
    CookedGraph cooked;
    ASSERT_TRUE(CompileGraphDefinition(f.graph, cooked));
    AnimationStatus status;
    EXPECT_EQ(GraphInstance::Create(cooked, f.rig, f.clips, status), nullptr);
    EXPECT_FALSE(status);
    f.graph.transitions[0].exitTime = 0.f;
    auto instance                   = f.Create();
    ASSERT_TRUE(instance);
    instance->SetPlaying(true);
    ASSERT_TRUE(instance->SetParameter(f.moving, true));
    ASSERT_TRUE(instance->Tick(0.f));
    EXPECT_EQ(instance->Debug().activeState, f.b);
}
