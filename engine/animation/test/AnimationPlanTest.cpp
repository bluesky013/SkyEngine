//
// Created by blues on 2026/9/21.
//

#include <animation/plan/AnimationPlan.h>
#include <animation/plan/AnimationPlanRuntime.h>
#include <animation/graph/AnimationClipNode.h>
#include <animation/graph/PoseBlendNode.h>
#include <animation/graph/AnimationState.h>
#include <animation/core/AnimationClip.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/Skeleton.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>
#include <string>

namespace {
    std::atomic<uint64_t> gAllocCount{0};
    std::atomic<bool> gCountAllocs{false};

    void* CountedAlloc(std::size_t size)
    {
        if (gCountAllocs.load(std::memory_order_relaxed)) {
            gAllocCount.fetch_add(1, std::memory_order_relaxed);
        }
        void* ptr = std::malloc(size);
        if (ptr == nullptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }
} // namespace

void* operator new(std::size_t size) { return CountedAlloc(size); }
void* operator new[](std::size_t size) { return CountedAlloc(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

using namespace sky;

namespace {

    SkeletonPtr MakePlanSkeleton()
    {
        SkeletonData data;
        data.boneData.emplace_back(BoneData{Name("Root")});
        data.refPos.emplace_back(Transform::GetIdentity());
        return Skeleton::BuildSkeleton(data);
    }

    AnimClipPtr MakePlanClip(const char* name, float y0, float y1)
    {
        AnimNodeChannelData channel;
        channel.name = "Root";
        channel.position.times = {0, 1, 2, 3};
        channel.position.keys = {
            Vector3(0.f, y0, 0.f),
            Vector3(0.f, y1, 0.f),
            Vector3(0.f, y1, 0.f),
            Vector3(0.f, y1, 0.f)
        };

        AnimClipPtr clip = new AnimationClip(Name(name));
        clip->AddChannel(new AnimationNodeChannel(channel));
        clip->SetFrameRate(1.f);
        return clip;
    }

    void ExpectPoseNear(const AnimPose& a, const AnimPose& b, float tol = 1e-4f)
    {
        ASSERT_EQ(a.transforms.size(), b.transforms.size());
        for (size_t i = 0; i < a.transforms.size(); ++i) {
            EXPECT_NEAR(a.transforms[i].translation.x, b.transforms[i].translation.x, tol);
            EXPECT_NEAR(a.transforms[i].translation.y, b.transforms[i].translation.y, tol);
            EXPECT_NEAR(a.transforms[i].translation.z, b.transforms[i].translation.z, tol);
        }
    }

    AnimPose MakePose(const SkeletonPtr& skeleton)
    {
        AnimPose pose;
        pose.skeleton = skeleton.Get();
        pose.transforms.resize(skeleton->GetNumBones());
        return pose;
    }

} // namespace

TEST(AnimationPlanTest, ClipGoldenPose)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clip = MakePlanClip("PlanClip", 0.f, 1.f);

    AnimationClipNode::PersistentData data;
    data.clip = clip;
    data.rootMotion = true;

    AnimationClipNode virtualNode(data);
    AnimContext context{};
    virtualNode.InitAny(context);
    virtualNode.TickAny({}, 0.25f);

    AnimationEval reference(skeleton);
    virtualNode.EvalAny(reference);

    AnimationClipNode planNode(data);
    planNode.SetPlaying(true);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&planNode, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(0.25f);

    AnimPose pose = MakePose(skeleton);
    runtime.Evaluate(pose);

    ExpectPoseNear(reference.pose, pose);
    EXPECT_NEAR(pose.transforms[0].translation.y, 0.25f, 1e-4f);
}

TEST(AnimationPlanTest, BlendGoldenPose)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clipA = MakePlanClip("PlanClipA", 0.f, 0.f);
    AnimClipPtr clipB = MakePlanClip("PlanClipB", 2.f, 2.f);

    AnimationClipNode::PersistentData dataA;
    dataA.clip = clipA;
    dataA.rootMotion = true;
    AnimationClipNode::PersistentData dataB;
    dataB.clip = clipB;
    dataB.rootMotion = true;

    AnimationClipNode nodeA(dataA);
    AnimationClipNode nodeB(dataB);
    nodeA.SetPlaying(true);
    nodeB.SetPlaying(true);

    PoseBlend2Node blend(&nodeA, &nodeB);
    blend.SetBlendTime(1.f);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&blend, *skeleton)) << runtime.Dump();
    runtime.Init();

    blend.SetBlend(false);
    runtime.Tick(0.5f);
    blend.SetBlend(true);
    runtime.Tick(0.5f);

    AnimPose pose = MakePose(skeleton);
    runtime.Evaluate(pose);

    EXPECT_NEAR(pose.transforms[0].translation.y, 1.f, 1e-4f);
}

TEST(AnimationPlanTest, StateMachineGoldenPose)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr idleClip = MakePlanClip("PlanIdle", 0.f, 0.f);
    AnimClipPtr walkClip = MakePlanClip("PlanWalk", 5.f, 5.f);

    AnimationClipNode::PersistentData dataIdle;
    dataIdle.clip = idleClip;
    dataIdle.rootMotion = true;
    AnimationClipNode::PersistentData dataWalk;
    dataWalk.clip = walkClip;
    dataWalk.rootMotion = true;

    AnimationClipNode idleNode(dataIdle);
    AnimationClipNode walkNode(dataWalk);
    idleNode.SetPlaying(true);
    walkNode.SetPlaying(true);

    TAnimFuncParameter<float> speed([](float) { return 1.f; });

    AnimStateMachine machine;
    AnimState idleState;
    idleState.name = Name("Idle");
    idleState.node = &idleNode;
    AnimState walkState;
    walkState.name = Name("Walk");
    walkState.node = &walkNode;

    AnimHandle idle = machine.AddState(idleState);
    AnimHandle walk = machine.AddState(walkState);
    AnimHandle condition = machine.AddCondition(new TAnimParameterCond<float>(&speed, 0.5f, AnimComp::GE));
    machine.AddTransition(AnimTransition{idle, walk, condition});
    machine.Finalize();

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&machine, *skeleton)) << runtime.Dump();
    runtime.Init();

    // idle active
    runtime.Tick(0.f);
    {
        AnimPose pose = MakePose(skeleton);
        runtime.Evaluate(pose);
        EXPECT_NEAR(pose.transforms[0].translation.y, 0.f, 1e-4f);
    }

    // transition to walk
    speed.Update(1.f);
    runtime.Tick(0.1f);
    {
        AnimPose pose = MakePose(skeleton);
        runtime.Evaluate(pose);
        EXPECT_NEAR(pose.transforms[0].translation.y, 5.f, 1e-4f);
    }
}

TEST(AnimationPlanTest, OpsAdditiveAndLayer)
{
    auto skeleton = MakePlanSkeleton();

    std::vector<Transform> bind = skeleton->GetRefPos()->transforms;
    AnimationPlanBuilder builder(static_cast<uint32_t>(skeleton->GetNumBones()), bind);

    AnimNodeChannelData base;
    base.name = "Root";
    base.position.times = {0};
    base.position.keys = {Vector3(0.f, 0.f, 0.f)};

    AnimNodeChannelData delta;
    delta.name = "Root";
    delta.position.times = {0};
    delta.position.keys = {Vector3(1.f, 0.f, 0.f)};

    AnimationTrackData baseTracks;
    baseTracks.AddChannelData(0, base.position, base.scale, base.rotation);
    AnimationTrackData deltaTracks;
    deltaTracks.AddChannelData(0, delta.position, delta.scale, delta.rotation);

    const uint32_t baseClip = builder.AddClip(baseTracks, 1.f);
    const uint32_t deltaClip = builder.AddClip(deltaTracks, 1.f);

    const uint32_t baseSlot = builder.AllocateSlot();
    AnimOpRecord baseOp;
    baseOp.op = AnimOp::ClipSample;
    baseOp.output = static_cast<uint16_t>(baseSlot);
    baseOp.dataIndex = baseClip;
    baseOp.weight = 1.f;
    builder.EmitOp(baseOp);

    const uint32_t deltaSlot = builder.AllocateSlot();
    AnimOpRecord deltaOp;
    deltaOp.op = AnimOp::ClipSample;
    deltaOp.output = static_cast<uint16_t>(deltaSlot);
    deltaOp.dataIndex = deltaClip;
    deltaOp.weight = 1.f;
    builder.EmitOp(deltaOp);

    const uint32_t addedSlot = builder.AllocateSlot();
    AnimOpRecord addOp;
    addOp.op = AnimOp::AdditiveBlend;
    addOp.inputA = static_cast<uint16_t>(baseSlot);
    addOp.inputB = static_cast<uint16_t>(deltaSlot);
    addOp.output = static_cast<uint16_t>(addedSlot);
    addOp.weight = 1.f;
    builder.EmitOp(addOp);

    const uint32_t layerSlot = builder.AllocateSlot();
    AnimOpRecord layerOp;
    layerOp.op = AnimOp::Layer;
    layerOp.inputA = static_cast<uint16_t>(addedSlot);
    layerOp.output = static_cast<uint16_t>(layerSlot);
    layerOp.weight = 1.f;
    builder.EmitOp(layerOp);

    AnimOpRecord outputOp;
    outputOp.op = AnimOp::Output;
    outputOp.output = static_cast<uint16_t>(layerSlot);
    builder.EmitOp(outputOp);
    builder.SetOutputSlot(layerSlot);

    AnimationPlan plan;
    ASSERT_TRUE(builder.Build(plan)) << (builder.GetDiagnostics().empty() ? "" : builder.GetDiagnostics().front());

    AnimFrameState state;
    state.Resize(plan.GetNumTimeSlots(), plan.GetNumWeightSlots(), plan.GetNumOps());
    AnimPose pose = MakePose(skeleton);
    plan.Evaluate(state, pose);

    EXPECT_NEAR(pose.transforms[0].translation.x, 1.f, 1e-4f);
}

TEST(AnimationPlanTest, EvaluationDoesNotAllocate)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clip = MakePlanClip("PlanAllocClip", 0.f, 1.f);

    AnimationClipNode::PersistentData data;
    data.clip = clip;
    data.rootMotion = true;
    AnimationClipNode node(data);
    node.SetPlaying(true);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&node, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(0.25f);

    AnimPose pose = MakePose(skeleton);
    runtime.Evaluate(pose);

    const uint64_t before = gAllocCount.load();
    gCountAllocs.store(true);
    for (int i = 0; i < 128; ++i) {
        runtime.Evaluate(pose);
    }
    gCountAllocs.store(false);

    ASSERT_EQ(gAllocCount.load(), before);
}

TEST(AnimationPlanTest, ValidationRejectsInvalidGraphs)
{
    auto skeleton = MakePlanSkeleton();
    std::vector<Transform> bind = skeleton->GetRefPos()->transforms;

    {
        AnimationPlanBuilder builder(static_cast<uint32_t>(skeleton->GetNumBones()), bind);

        const uint32_t slot = builder.AllocateSlot();
        AnimOpRecord sample;
        sample.op = AnimOp::ClipSample;
        sample.output = static_cast<uint16_t>(slot);
        sample.dataIndex = 999;
        builder.EmitOp(sample);

        const uint32_t output = builder.AllocateSlot();
        AnimOpRecord outputOp;
        outputOp.op = AnimOp::Output;
        outputOp.output = static_cast<uint16_t>(output);
        builder.EmitOp(outputOp);
        builder.SetOutputSlot(output);

        AnimationPlan plan;
        ASSERT_FALSE(builder.Build(plan));
        ASSERT_FALSE(builder.GetDiagnostics().empty());
    }

    {
        AnimationPlanBuilder builder(static_cast<uint32_t>(skeleton->GetNumBones()), bind);

        const uint32_t s0 = builder.AllocateSlot();
        const uint32_t s1 = builder.AllocateSlot();

        AnimOpRecord blend;
        blend.op = AnimOp::Blend;
        blend.inputA = static_cast<uint16_t>(s0);
        blend.inputB = static_cast<uint16_t>(s1);
        blend.output = static_cast<uint16_t>(s1);
        builder.EmitOp(blend);

        AnimOpRecord outputOp;
        outputOp.op = AnimOp::Output;
        outputOp.output = static_cast<uint16_t>(s1);
        builder.EmitOp(outputOp);
        builder.SetOutputSlot(s1);

        AnimationPlan plan;
        ASSERT_FALSE(builder.Build(plan));
        ASSERT_FALSE(builder.GetDiagnostics().empty());
    }
}

TEST(AnimationPlanTest, SerializationRoundTrip)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clipA = MakePlanClip("PlanSerA", 0.f, 1.f);
    AnimClipPtr clipB = MakePlanClip("PlanSerB", 2.f, 3.f);

    AnimationClipNode::PersistentData dataA;
    dataA.clip = clipA;
    dataA.rootMotion = true;
    AnimationClipNode::PersistentData dataB;
    dataB.clip = clipB;
    dataB.rootMotion = true;

    AnimationClipNode nodeA(dataA);
    AnimationClipNode nodeB(dataB);
    nodeA.SetPlaying(true);
    nodeB.SetPlaying(true);

    PoseBlend2Node blend(&nodeA, &nodeB);
    blend.SetBlendTime(1.f);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&blend, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(0.5f);

    const std::vector<uint8_t> blob = AnimationPlanBlob::Serialize(runtime.GetPlan());

    AnimationPlan restored;
    ASSERT_TRUE(AnimationPlanBlob::Deserialize(blob, restored));

    AnimPose original = MakePose(skeleton);
    runtime.GetPlan().Evaluate(runtime.GetFrameState(), original);

    AnimPose roundTrip = MakePose(skeleton);
    restored.Evaluate(runtime.GetFrameState(), roundTrip);

    ExpectPoseNear(original, roundTrip);
}

TEST(AnimationPlanTest, BoneMaskExcludesTracks)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clip = MakePlanClip("PlanMaskClip", 1.f, 1.f);

    AnimationClipNode::PersistentData data;
    data.clip = clip;
    data.rootMotion = true;

    // masked-out bone keeps the bind pose
    AnimationClipNode masked(data);
    masked.SetPlaying(true);
    AnimationBoneMask mask{AnimationBoneMask::MaskFull{}};
    mask.ResetBit(0);
    masked.SetBoneMask(mask);

    AnimationPlanRuntime maskedRuntime;
    ASSERT_TRUE(maskedRuntime.Compile(&masked, *skeleton)) << maskedRuntime.Dump();
    maskedRuntime.Init();
    maskedRuntime.Tick(0.f);

    AnimPose maskedPose = MakePose(skeleton);
    maskedRuntime.Evaluate(maskedPose);
    EXPECT_NEAR(maskedPose.transforms[0].translation.y, 0.f, 1e-4f);

    // unmasked samples the clip
    AnimationClipNode unmasked(data);
    unmasked.SetPlaying(true);

    AnimationPlanRuntime unmaskedRuntime;
    ASSERT_TRUE(unmaskedRuntime.Compile(&unmasked, *skeleton)) << unmaskedRuntime.Dump();
    unmaskedRuntime.Init();
    unmaskedRuntime.Tick(0.f);

    AnimPose unmaskedPose = MakePose(skeleton);
    unmaskedRuntime.Evaluate(unmaskedPose);
    EXPECT_NEAR(unmaskedPose.transforms[0].translation.y, 1.f, 1e-4f);
}

TEST(AnimationPlanTest, RootMotionDeltaOutput)
{
    auto skeleton = MakePlanSkeleton();

    AnimNodeChannelData data;
    data.name = "Root";
    data.position.times = {0, 1};
    data.position.keys = {Vector3(0.f, 0.f, 0.f), Vector3(2.f, 0.f, 0.f)};

    AnimClipPtr clip = new AnimationClip(Name("PlanRootDelta"));
    clip->AddChannel(new AnimationNodeChannel(data));
    clip->SetFrameRate(1.f);

    AnimationClipNode::PersistentData persistent;
    persistent.clip = clip;
    persistent.rootMotion = false;

    AnimationClipNode node(persistent);
    node.SetPlaying(true);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&node, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(1.f);

    AnimPose pose = MakePose(skeleton);
    Transform delta;
    runtime.Evaluate(pose, &delta);

    EXPECT_NEAR(pose.transforms[0].translation.x, 0.f, 1e-4f);
    EXPECT_NEAR(delta.translation.x, 2.f, 1e-4f);
}

TEST(AnimationPlanTest, DynamicListWeights)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr clipA = MakePlanClip("PlanDynA", 0.f, 0.f);
    AnimClipPtr clipB = MakePlanClip("PlanDynB", 2.f, 2.f);

    AnimationClipNode::PersistentData dataA;
    dataA.clip = clipA;
    dataA.rootMotion = true;
    AnimationClipNode::PersistentData dataB;
    dataB.clip = clipB;
    dataB.rootMotion = true;

    AnimationClipNode nodeA(dataA);
    AnimationClipNode nodeB(dataB);
    nodeA.SetPlaying(true);
    nodeB.SetPlaying(true);

    PoseBlendNodeList list;
    list.AddPose(&nodeA, 1.f);
    list.AddPose(&nodeB, 1.f);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&list, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(0.f);

    {
        AnimPose pose = MakePose(skeleton);
        runtime.Evaluate(pose);
        EXPECT_NEAR(pose.transforms[0].translation.y, 1.f, 1e-4f);
    }

    list.SetPoseWeight(0, 1.f);
    list.SetPoseWeight(1, 0.f);
    runtime.Tick(0.f);
    {
        AnimPose pose = MakePose(skeleton);
        runtime.Evaluate(pose);
        EXPECT_NEAR(pose.transforms[0].translation.y, 0.f, 1e-4f);
    }

    list.SetPoseWeight(0, 0.f);
    list.SetPoseWeight(1, 1.f);
    runtime.Tick(0.f);
    {
        AnimPose pose = MakePose(skeleton);
        runtime.Evaluate(pose);
        EXPECT_NEAR(pose.transforms[0].translation.y, 2.f, 1e-4f);
    }
}

TEST(AnimationPlanTest, AdditiveBlendEmission)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr baseClip = MakePlanClip("PlanAddBase", 0.f, 0.f);
    AnimClipPtr deltaClip = MakePlanClip("PlanAddDelta", 1.f, 1.f);

    AnimationClipNode::PersistentData baseData;
    baseData.clip = baseClip;
    baseData.rootMotion = true;
    AnimationClipNode::PersistentData deltaData;
    deltaData.clip = deltaClip;
    deltaData.rootMotion = true;

    AnimationClipNode baseNode(baseData);
    AnimationClipNode deltaNode(deltaData);
    baseNode.SetPlaying(true);
    deltaNode.SetPlaying(true);

    PoseBlend2Node blend(&baseNode, &deltaNode);
    blend.SetAdditive(true);
    blend.SetBlendTime(0.f);
    blend.SetBlend(true);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&blend, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(1.f);

    bool hasAdditive = false;
    for (const AnimOpRecord& op : runtime.GetPlan().GetOps()) {
        if (op.op == AnimOp::AdditiveBlend) {
            hasAdditive = true;
        }
    }
    EXPECT_TRUE(hasAdditive);

    AnimPose pose = MakePose(skeleton);
    runtime.Evaluate(pose);
    EXPECT_NEAR(pose.transforms[0].translation.y, 1.f, 1e-4f);
}

TEST(AnimationPlanTest, LayerWeightSemantics)
{
    auto skeleton = MakePlanSkeleton();

    AnimNodeChannelData src;
    src.name = "Root";
    src.position.times = {0};
    src.position.keys = {Vector3(0.f, 4.f, 0.f)};

    AnimationTrackData tracks;
    tracks.AddChannelData(0, src.position, src.scale, src.rotation);

    std::vector<Transform> bind = skeleton->GetRefPos()->transforms;
    AnimationPlanBuilder builder(static_cast<uint32_t>(skeleton->GetNumBones()), bind);

    const uint32_t clipIndex = builder.AddClip(tracks, 1.f);

    const uint32_t clipSlot = builder.AllocateSlot();
    AnimOpRecord sampleOp;
    sampleOp.op = AnimOp::ClipSample;
    sampleOp.output = static_cast<uint16_t>(clipSlot);
    sampleOp.dataIndex = clipIndex;
    sampleOp.weight = 1.f;
    builder.EmitOp(sampleOp);

    const uint32_t layerSlot = builder.AllocateSlot();
    AnimOpRecord layerOp;
    layerOp.op = AnimOp::Layer;
    layerOp.inputA = static_cast<uint16_t>(clipSlot);
    layerOp.output = static_cast<uint16_t>(layerSlot);
    layerOp.weight = 0.5f;
    builder.EmitOp(layerOp);

    AnimOpRecord outputOp;
    outputOp.op = AnimOp::Output;
    outputOp.output = static_cast<uint16_t>(layerSlot);
    builder.EmitOp(outputOp);
    builder.SetOutputSlot(layerSlot);

    AnimationPlan plan;
    ASSERT_TRUE(builder.Build(plan)) << (builder.GetDiagnostics().empty() ? "" : builder.GetDiagnostics().front());

    AnimFrameState state;
    state.Resize(plan.GetNumTimeSlots(), plan.GetNumWeightSlots(), plan.GetNumOps());
    AnimPose pose = MakePose(skeleton);
    plan.Evaluate(state, pose);

    // bind y=0, source y=4, layer weight 0.5 -> 2
    EXPECT_NEAR(pose.transforms[0].translation.y, 2.f, 1e-4f);
}

TEST(AnimationPlanTest, InactiveStateOpsDisabled)
{
    auto skeleton = MakePlanSkeleton();
    AnimClipPtr idleClip = MakePlanClip("PlanGateIdle", 0.f, 0.f);
    AnimClipPtr walkClip = MakePlanClip("PlanGateWalk", 5.f, 5.f);

    AnimationClipNode::PersistentData idleData;
    idleData.clip = idleClip;
    idleData.rootMotion = true;
    AnimationClipNode::PersistentData walkData;
    walkData.clip = walkClip;
    walkData.rootMotion = true;

    AnimationClipNode idleNode(idleData);
    AnimationClipNode walkNode(walkData);
    idleNode.SetPlaying(true);
    walkNode.SetPlaying(true);

    AnimStateMachine machine;
    AnimState idleState;
    idleState.name = Name("Idle");
    idleState.node = &idleNode;
    AnimState walkState;
    walkState.name = Name("Walk");
    walkState.node = &walkNode;
    machine.AddState(idleState);
    machine.AddState(walkState);
    machine.Finalize();

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&machine, *skeleton)) << runtime.Dump();
    runtime.Init();
    runtime.Tick(0.f);

    const AnimFrameState& state = runtime.GetFrameState();
    uint32_t enabled = 0;
    for (uint8_t flag : state.opEnabled) {
        enabled += flag != 0 ? 1u : 0u;
    }

    // idle state ops + StateSelect + Output only
    EXPECT_LT(enabled, runtime.GetPlan().GetNumOps());
    EXPECT_GE(enabled, 3u);
}

namespace {

    SkeletonPtr MakeBenchSkeleton(uint32_t numBones)
    {
        SkeletonData data;
        data.boneData.emplace_back(BoneData{Name("B0")});
        data.refPos.emplace_back(Transform::GetIdentity());

        for (uint32_t i = 1; i < numBones; ++i) {
            data.boneData.emplace_back(BoneData{Name(("B" + std::to_string(i)).c_str()), 0});
            data.refPos.emplace_back(Transform::GetIdentity());
        }
        return Skeleton::BuildSkeleton(data);
    }

    AnimClipPtr MakeBenchClip(const char* name, uint32_t numBones, float base)
    {
        AnimClipPtr clip = new AnimationClip(Name(name));

        for (uint32_t i = 0; i < numBones; ++i) {
            AnimNodeChannelData channel;
            channel.name = "B" + std::to_string(i);
            channel.position.times = {0, 1, 2, 3};
            channel.position.keys = {
                Vector3(0.f, base + 0.f, 0.f),
                Vector3(0.f, base + 1.f, 0.f),
                Vector3(0.f, base + 2.f, 0.f),
                Vector3(0.f, base + 3.f, 0.f)
            };
            channel.rotation.times = {0, 1, 2, 3};
            channel.rotation.keys = {Quaternion{}, Quaternion{}, Quaternion{}, Quaternion{}};
            clip->AddChannel(new AnimationNodeChannel(channel));
        }

        clip->SetFrameRate(1.f);
        return clip;
    }

    template <typename Fn>
    double TimePerEval(int iterations, Fn&& fn)
    {
        for (int i = 0; i < 32; ++i) {
            fn();
        }

        const auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            fn();
        }
        const auto end = std::chrono::high_resolution_clock::now();

        return std::chrono::duration<double, std::nano>(end - start).count() / static_cast<double>(iterations);
    }

    struct BenchSetup {
        SkeletonPtr skeleton;
        AnimationClipNode::PersistentData dataA;
        AnimationClipNode::PersistentData dataB;
    };

    BenchSetup MakeBenchSetup(uint32_t numBones)
    {
        BenchSetup setup;
        setup.skeleton = MakeBenchSkeleton(numBones);
        setup.dataA.clip = MakeBenchClip("BenchA", numBones, 0.f);
        setup.dataA.rootMotion = true;
        setup.dataB.clip = MakeBenchClip("BenchB", numBones, 10.f);
        setup.dataB.rootMotion = true;
        return setup;
    }

} // namespace

TEST(AnimationPlanTest, PerformanceAllocationComparison)
{
    const uint32_t numBones = 16;
    BenchSetup setup = MakeBenchSetup(numBones);

    // virtual path
    AnimationClipNode virtualA(setup.dataA);
    AnimationClipNode virtualB(setup.dataB);
    PoseBlend2Node virtualBlend(&virtualA, &virtualB);

    AnimContext context{};
    virtualBlend.InitAny(context);
    virtualBlend.SetBlendTime(1.f);
    virtualBlend.SetBlend(false);
    virtualBlend.TickAny({}, 0.5f);
    virtualBlend.SetBlend(true);
    virtualBlend.TickAny({}, 0.5f);

    AnimationEval eval(setup.skeleton);

    const int iterations = 512;

    const uint64_t virtualBefore = gAllocCount.load();
    gCountAllocs.store(true);
    for (int i = 0; i < iterations; ++i) {
        virtualBlend.EvalAny(eval);
    }
    gCountAllocs.store(false);
    const uint64_t virtualAllocs = gAllocCount.load() - virtualBefore;

    // plan path
    AnimationClipNode planA(setup.dataA);
    AnimationClipNode planB(setup.dataB);
    planA.SetPlaying(true);
    planB.SetPlaying(true);

    PoseBlend2Node planBlend(&planA, &planB);
    planBlend.SetBlendTime(1.f);

    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&planBlend, *setup.skeleton)) << runtime.Dump();
    runtime.Init();
    planBlend.SetBlend(false);
    runtime.Tick(0.5f);
    planBlend.SetBlend(true);
    runtime.Tick(0.5f);

    AnimPose pose = MakePose(setup.skeleton);

    const uint64_t planBefore = gAllocCount.load();
    gCountAllocs.store(true);
    for (int i = 0; i < iterations; ++i) {
        runtime.Evaluate(pose);
    }
    gCountAllocs.store(false);
    const uint64_t planAllocs = gAllocCount.load() - planBefore;

    std::cout << "[ PERF ] allocations over " << iterations
              << " evals (" << numBones << " bones): virtual=" << virtualAllocs
              << " plan=" << planAllocs << std::endl;

    EXPECT_GT(virtualAllocs, 0u);
    EXPECT_EQ(planAllocs, 0u);
}

TEST(AnimationPlanTest, PerformanceEvaluationTime)
{
    const uint32_t boneCounts[] = {16, 64, 128};
    const int iterations = 20000;

    for (uint32_t numBones : boneCounts) {
        BenchSetup setup = MakeBenchSetup(numBones);

        // virtual path
        AnimationClipNode virtualA(setup.dataA);
        AnimationClipNode virtualB(setup.dataB);
        PoseBlend2Node virtualBlend(&virtualA, &virtualB);

        AnimContext context{};
        virtualBlend.InitAny(context);
        virtualBlend.SetBlendTime(1.f);
        virtualBlend.SetBlend(false);
        virtualBlend.TickAny({}, 0.5f);
        virtualBlend.SetBlend(true);
        virtualBlend.TickAny({}, 0.5f);

        AnimationEval eval(setup.skeleton);

        // plan path
        AnimationClipNode planA(setup.dataA);
        AnimationClipNode planB(setup.dataB);
        planA.SetPlaying(true);
        planB.SetPlaying(true);

        PoseBlend2Node planBlend(&planA, &planB);
        planBlend.SetBlendTime(1.f);

        AnimationPlanRuntime runtime;
        ASSERT_TRUE(runtime.Compile(&planBlend, *setup.skeleton)) << runtime.Dump();
        runtime.Init();
        planBlend.SetBlend(false);
        runtime.Tick(0.5f);
        planBlend.SetBlend(true);
        runtime.Tick(0.5f);

        AnimPose pose = MakePose(setup.skeleton);

        const double virtualNs = TimePerEval(iterations, [&]() { virtualBlend.EvalAny(eval); });
        const double planNs = TimePerEval(iterations, [&]() { runtime.Evaluate(pose); });

        std::cout << "[ PERF ] ns/eval (" << numBones << " bones): virtual="
                  << virtualNs << " plan=" << planNs
                  << " ratio=" << (virtualNs / planNs) << "x" << std::endl;

        RecordProperty("virtual_ns_per_eval_" + std::to_string(numBones), static_cast<int64_t>(virtualNs));
        RecordProperty("plan_ns_per_eval_" + std::to_string(numBones), static_cast<int64_t>(planNs));

        // loose regression guard: the flat evaluator should not be slower than the
        // virtual evaluator by more than a wide margin (Debug builds are unoptimized)
        EXPECT_LT(planNs, virtualNs * 2.0 + 500.0);
    }
}
