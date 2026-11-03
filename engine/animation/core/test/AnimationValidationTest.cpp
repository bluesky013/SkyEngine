#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationPlayer.h>
#include <animation/core/AnimationUtils.h>
#include <animation/core/Skeleton.h>
#include <animation/graph/AnimationClipNode.h>
#include <animation/plan/AnimationPlanRuntime.h>

#include <gtest/gtest.h>
#include <limits>

using namespace sky;

namespace {
    SkeletonData TwoBoneRig()
    {
        SkeletonData data;
        data.boneData = {{Name("root"), INVALID_BONE_ID}, {Name("root/helper"), 0}};
        data.refPos.resize(2);
        data.refPos[1].translation = Vector3(0.f, 2.f, 0.f);
        return data;
    }

    AnimClipPtr TranslationClip()
    {
        AnimNodeChannelData channel;
        channel.name           = "root";
        channel.position.times = {0, 1};
        channel.position.keys  = {Vector3(0.f), Vector3(1.f, 0.f, 0.f)};
        AnimClipPtr clip       = new AnimationClip(Name("translation"));
        clip->AddChannel(new AnimationNodeChannel(channel));
        clip->SetFrameRate(1.f);
        return clip;
    }
} // namespace

TEST(AnimationValidation, RejectsInvalidSkeletonWithoutAsserting)
{
    auto            data = TwoBoneRig();
    AnimationStatus status;
    ASSERT_NE(Skeleton::BuildSkeleton(data, &status), nullptr);
    ASSERT_TRUE(status);
    data.boneData[0].parentIndex = 1;
    EXPECT_EQ(Skeleton::BuildSkeleton(data, &status), nullptr);
    EXPECT_EQ(status.error, AnimationError::InvalidSkeleton);
    data                         = TwoBoneRig();
    data.boneData[1].parentIndex = 2;
    EXPECT_FALSE(Skeleton::Validate(data));
    data                  = TwoBoneRig();
    data.boneData[1].name = data.boneData[0].name;
    EXPECT_FALSE(Skeleton::Validate(data));
    data = TwoBoneRig();
    data.refPos.pop_back();
    EXPECT_FALSE(Skeleton::Validate(data));
    data                         = TwoBoneRig();
    data.refPos[0].translation.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(Skeleton::Validate(data));
    data                   = TwoBoneRig();
    data.refPos[0].scale.x = 0.f;
    EXPECT_FALSE(Skeleton::Validate(data));
    data                    = TwoBoneRig();
    data.refPos[0].rotation = Quaternion(0.f, 0.f, 0.f, 0.f);
    EXPECT_FALSE(Skeleton::Validate(data));
    EXPECT_FALSE(Skeleton::Validate({}));
    data.boneData.resize(ANIM_MAX_BONE + 1);
    data.refPos.resize(ANIM_MAX_BONE + 1);
    EXPECT_FALSE(Skeleton::Validate(data));
}

TEST(AnimationValidation, LayoutSignatureIncludesIdentityHierarchyAndReferencePose)
{
    auto       data     = TwoBoneRig();
    const auto original = Skeleton::BuildSkeleton(data);
    ASSERT_NE(original, nullptr);
    EXPECT_EQ(original->GetLayoutSignature(), Skeleton::BuildSkeleton(data)->GetLayoutSignature());
    data.refPos[1].translation.y = 3.f;
    EXPECT_NE(original->GetLayoutSignature(), Skeleton::BuildSkeleton(data)->GetLayoutSignature());
    data                         = TwoBoneRig();
    data.boneData[1].parentIndex = INVALID_BONE_ID;
    EXPECT_NE(original->GetLayoutSignature(), Skeleton::BuildSkeleton(data)->GetLayoutSignature());
    data                  = TwoBoneRig();
    data.boneData[1].name = Name("root/other");
    EXPECT_NE(original->GetLayoutSignature(), Skeleton::BuildSkeleton(data)->GetLayoutSignature());
}

TEST(AnimationValidation, RejectsInvalidChannelShapeTimesAndValues)
{
    AnimNodeChannelData channel;
    channel.name           = "root";
    channel.position.times = {0, 2};
    channel.position.keys  = {Vector3(0.f)};
    EXPECT_FALSE(channel.Validate());
    channel.position.keys.push_back(Vector3(1.f));
    EXPECT_TRUE(channel.Validate());
    channel.position.times = {2, 2};
    EXPECT_FALSE(channel.Validate());
    channel.position.times = {-1, 2};
    EXPECT_FALSE(channel.Validate());
    channel.position.times = {2, 1};
    EXPECT_FALSE(channel.Validate());
    channel.position.times     = {0, 2};
    channel.position.keys[0].x = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(channel.Validate());
    const auto clip = TranslationClip();
    EXPECT_FALSE(clip->AddChannel(new AnimationNodeChannel(channel)));
    channel.position.keys[0].x = 0.f;
    EXPECT_FALSE(clip->AddChannel(new AnimationNodeChannel(channel)));
    channel.name = "unknown";
    EXPECT_TRUE(clip->AddChannel(new AnimationNodeChannel(channel)));
    const auto rig = Skeleton::BuildSkeleton(TwoBoneRig());
    EXPECT_FALSE(clip->Validate(rig.Get()));
}

TEST(AnimationValidation, ExplicitDurationSurvivesCompressedTail)
{
    AnimNodeChannelData channel;
    channel.name           = "root";
    channel.position.times = {0, 5, 60};
    channel.position.keys  = {Vector3(1.f), Vector3(1.f), Vector3(1.f)};
    const auto frameCount  = channel.GetFrameCount();
    channel.Compress();
    ASSERT_EQ(channel.position.times.size(), 1U);
    AnimationClip clip(Name("constant"));
    ASSERT_TRUE(clip.AddChannel(new AnimationNodeChannel(channel)));
    clip.SetFrameRate(30.f);
    clip.SetNumFrame(frameCount);
    EXPECT_EQ(clip.GetFrameCount(), 61U);
    EXPECT_FLOAT_EQ(clip.GetDuration(), 61.f / 30.f);
    const auto sparse = TranslationClip();
    sparse->SetNumFrame(1);
    EXPECT_FALSE(sparse->Validate());
}

TEST(AnimationValidation, PlaybackPauseSeekLoopAndInvalidInputs)
{
    AnimationSequencePlayer player;
    EXPECT_FALSE(player.Tick(0.1f));
    EXPECT_FALSE(player.Seek(0.f));
    ASSERT_TRUE(player.SetClip(TranslationClip()));
    ASSERT_TRUE(player.Tick(1.f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.f);
    ASSERT_TRUE(player.Seek(2.f));
    EXPECT_FALSE(player.IsPlaying());
    player.SetLoop(true);
    ASSERT_TRUE(player.SetPlaying(true));
    ASSERT_TRUE(player.Tick(0.f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 2.f);
    ASSERT_TRUE(player.Tick(6.5f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.5f);
    EXPECT_FALSE(player.SetPlayRate(-1.f));
    EXPECT_FALSE(player.SetPlayRate(std::numeric_limits<float>::infinity()));
    EXPECT_FALSE(player.Tick(-1.f));
    EXPECT_FALSE(player.Tick(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FALSE(player.Seek(std::numeric_limits<float>::quiet_NaN()));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.5f);
    ASSERT_TRUE(player.SetPlayRate(0.f));
    ASSERT_TRUE(player.Tick(100.f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.5f);
    player.Stop();
    EXPECT_FALSE(player.IsPlaying());
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.f);
    ASSERT_TRUE(player.Seek(-20.f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.f);
    AnimClipPtr empty = new AnimationClip(Name("empty"));
    ASSERT_TRUE(player.SetClip(empty));
    ASSERT_TRUE(player.SetPlaying(true));
    ASSERT_TRUE(player.Tick(1.f));
    EXPECT_FLOAT_EQ(player.GetCurrentTime(), 0.f);
    EXPECT_FALSE(player.IsPlaying());
}

TEST(AnimationValidation, VirtualAndPlanKeepPausedInitAndRefillPartialChannels)
{
    const auto                        rig  = Skeleton::BuildSkeleton(TwoBoneRig());
    const auto                        clip = TranslationClip();
    AnimationClipNode::PersistentData data;
    data.clip       = clip;
    data.rootMotion = true;
    AnimationClipNode node(data);
    node.SetPlaying(false);
    EXPECT_TRUE(node.GetStatus());
    ASSERT_TRUE(node.SetPlayRate(0.5f));
    node.InitAny({});
    node.TickAny({}, 1.f);
    EXPECT_FALSE(node.IsPlaying());
    EXPECT_FLOAT_EQ(node.GetPlayerTime(), 0.f);
    ASSERT_TRUE(node.Seek(1.f));
    AnimationEval eval(rig);
    eval.pose.transforms[0].scale       = Vector3(5.f);
    eval.pose.transforms[1].translation = Vector3(99.f);
    node.EvalAny(eval);
    ASSERT_TRUE(node.GetStatus());
    EXPECT_FLOAT_EQ(eval.pose.transforms[0].translation.x, 1.f);
    EXPECT_FLOAT_EQ(eval.pose.transforms[0].scale.x, 1.f);
    EXPECT_FLOAT_EQ(eval.pose.transforms[1].translation.y, 2.f);
    node.Stop();
    node.SetPlaying(true);
    node.TickAny({}, 1.f);
    EXPECT_FLOAT_EQ(node.GetPlayerTime(), 0.5f);

    AnimationClipNode planNode(data);
    planNode.SetPlaying(false);
    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&planNode, *rig));
    runtime.Init();
    runtime.Tick(1.f);
    EXPECT_FALSE(planNode.IsPlaying());
    EXPECT_FLOAT_EQ(planNode.GetPlayerTime(), 0.f);
    ASSERT_TRUE(planNode.Seek(1.f));
    EXPECT_FALSE(runtime.Tick(-1.f));
    EXPECT_FALSE(runtime.Tick(std::numeric_limits<float>::infinity()));
    AnimPose pose = *rig->GetRefPos();
    runtime.Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 1.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 2.f);
    planNode.Stop();
    runtime.Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].translation.x, 0.f);
    EXPECT_FALSE(planNode.IsPlaying());
}

TEST(AnimationValidation, ModelMatricesPreserveNonuniformScaleAndOutOfOrderBones)
{
    auto data               = TwoBoneRig();
    data.refPos[0].scale    = Vector3(2.f, 3.f, 4.f);
    data.refPos[1].rotation = Quaternion(0.7f, Vector3(0.f, 0.f, 1.f));
    std::swap(data.boneData[0], data.boneData[1]);
    std::swap(data.refPos[0], data.refPos[1]);
    data.boneData[0].parentIndex = 1;
    const auto rig               = Skeleton::BuildSkeleton(data);
    ASSERT_NE(rig, nullptr);
    AnimPose  pose = *rig->GetRefPos();
    Transform world;
    world.translation = Vector3(4.f, 5.f, 6.f);
    std::vector<Matrix4> matrices;
    ASSERT_TRUE(pose.ToSkinRenderData(matrices, world));
    const Matrix4 expected = world.ToMatrix() * data.refPos[1].ToMatrix() * data.refPos[0].ToMatrix();
    for (uint32_t column = 0; column < 4; ++column) {
        for (uint32_t row = 0; row < 4; ++row) {
            EXPECT_NEAR(matrices[0][column][row], expected[column][row], 1e-5f);
        }
    }
    pose.transforms.clear();
    EXPECT_FALSE(pose.ToSkinRenderData(matrices, world));
}

TEST(AnimationValidation, FrameConversionClampsEndpointBeyondSignedKeyRange)
{
    const auto frame = Anim::ConvertFromFrameRate(2147483648.f, 1.f);
    EXPECT_EQ(frame.frame, std::numeric_limits<AnimTimeKey>::max());
    EXPECT_FLOAT_EQ(frame.subFrame, 0.f);
}

TEST(AnimationValidation, CompressionKeepsPlateauBeforeMovement)
{
    AnimNodeChannelData channel;
    channel.name           = "root";
    channel.position.times = {0, 5, 10, 20};
    channel.position.keys  = {Vector3(0.f), Vector3(0.f), Vector3(1.f), Vector3(1.f)};
    const auto original    = channel;
    ASSERT_TRUE(channel.Compress());
    ASSERT_EQ(channel.position.times.size(), 3U);
    for (int frame = 0; frame <= 20; ++frame) {
        const SampleParam param = {AnimFrameTime{frame, 0.f}, AnimInterpolation::LINEAR};
        EXPECT_FLOAT_EQ(AnimSampleChannel(channel.position, param).x, AnimSampleChannel(original.position, param).x);
    }
    channel.position.keys.pop_back();
    EXPECT_FALSE(channel.Compress());
}

TEST(AnimationValidation, SwitchingClipsAndReusingPlanRestoresMissingHelperAndScaleTracks)
{
    const auto  rig   = Skeleton::BuildSkeleton(TwoBoneRig());
    AnimClipPtr dense = new AnimationClip(Name("dense"));
    dense->SetFrameRate(1.f);
    AnimNodeChannelData root;
    root.name        = "root";
    root.scale.times = {0};
    root.scale.keys  = {Vector3(3.f)};
    ASSERT_TRUE(dense->AddChannel(new AnimationNodeChannel(root)));
    AnimNodeChannelData helper;
    helper.name           = "root/helper";
    helper.position.times = {0};
    helper.position.keys  = {Vector3(9.f)};
    ASSERT_TRUE(dense->AddChannel(new AnimationNodeChannel(helper)));
    const auto        partial = TranslationClip();
    AnimPose          pose    = *rig->GetRefPos();
    const SampleParam sample{{0, 0.f}, AnimInterpolation::LINEAR};
    ASSERT_TRUE(dense->SamplePose(pose, sample));
    EXPECT_FLOAT_EQ(pose.transforms[0].scale.x, 3.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 9.f);
    ASSERT_TRUE(partial->SamplePose(pose, sample));
    EXPECT_FLOAT_EQ(pose.transforms[0].scale.x, 1.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 2.f);
    AnimationClipNode::PersistentData data;
    data.clip       = dense;
    data.rootMotion = true;
    AnimationClipNode denseNode(data);
    data.clip = partial;
    AnimationClipNode    partialNode(data);
    AnimationPlanRuntime runtime;
    ASSERT_TRUE(runtime.Compile(&denseNode, *rig));
    runtime.Init();
    runtime.Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].scale.x, 3.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 9.f);
    ASSERT_TRUE(runtime.Compile(&partialNode, *rig));
    runtime.Init();
    runtime.Evaluate(pose);
    EXPECT_FLOAT_EQ(pose.transforms[0].scale.x, 1.f);
    EXPECT_FLOAT_EQ(pose.transforms[1].translation.y, 2.f);
}
