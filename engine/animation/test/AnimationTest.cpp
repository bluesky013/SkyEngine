//
// Created by Zach Lee on 2025/6/2.
//

#include "animation/core/Skeleton.h"

#include <animation/core/Animation.h>
#include <animation/core/AnimationCondition.h>
#include <animation/core/AnimationInterpolation.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationPlayer.h>
#include <animation/core/AnimationUtils.h>
#include <animation/graph/AnimationClipNode.h>
#include <animation/graph/PoseBlendNode.h>
#include <gtest/gtest.h>
using namespace sky;

TEST(AnimationTest, AnimationChannelFloatCompressTest)
{
    AnimChannelData<float> data = {
            {0, 1, 2, 3},
            {3.0f, 3.5f, 3.5f + ANIM_DIFF_TOLERANCE, 4.0f}
    };

    data.Compress();

    ASSERT_EQ(data.times.size(), 3);
    ASSERT_EQ(data.keys.size(), 3);

    ASSERT_EQ(data.times[0], 0);
    ASSERT_EQ(data.times[1], 1);
    ASSERT_EQ(data.times[2], 3);

    ASSERT_FLOAT_EQ(data.keys[0], 3.0f);
    ASSERT_FLOAT_EQ(data.keys[1], 3.5f);
    ASSERT_FLOAT_EQ(data.keys[2], 4.0f);
}

TEST(AnimationTest, AnimationChannelVecCompressTest)
{
    AnimChannelData<Vector2> data = {
        {0, 1, 2, 3},
        {Vector2(3.0f), Vector2(3.5f), Vector2(3.5f + ANIM_DIFF_TOLERANCE), Vector2(4.0f)}
    };

    data.Compress();

    ASSERT_EQ(data.times.size(), 3);
    ASSERT_EQ(data.keys.size(), 3);

    ASSERT_EQ(data.times[0], 0);
    ASSERT_EQ(data.times[1], 1);
    ASSERT_EQ(data.times[2], 3);

    ASSERT_FLOAT_EQ(data.keys[0].x, 3.0f);
    ASSERT_FLOAT_EQ(data.keys[1].x, 3.5f);
    ASSERT_FLOAT_EQ(data.keys[2].x, 4.0f);

    ASSERT_FLOAT_EQ(data.keys[0].y, 3.0f);
    ASSERT_FLOAT_EQ(data.keys[1].y, 3.5f);
    ASSERT_FLOAT_EQ(data.keys[2].y, 4.0f);

}

TEST(AnimationTest, AnimationChannelDataTest)
{
    AnimChannelData<float> data = {
            {1, 2, 3, 4},
            {3.0f, 4.0f, 5.0f, 6.0f}
    };

    auto k1 = data.FindKeyFrame(-1);
    ASSERT_EQ(k1.first, 0);
    ASSERT_EQ(k1.second, 0);

    auto k2 = data.FindKeyFrame(3);
    ASSERT_EQ(k2.first, 2);
    ASSERT_EQ(k2.second, 3);

    auto k3 = data.FindKeyFrame(1);
    ASSERT_EQ(k3.first, 0);
    ASSERT_EQ(k3.second, 1);

    auto k4 = data.FindKeyFrame(0);
    ASSERT_EQ(k4.first, 0);
    ASSERT_EQ(k4.second, 0);

    auto k5 = data.FindKeyFrame(5);
    ASSERT_EQ(k5.first, 3);
    ASSERT_EQ(k5.second, 3);
}

TEST(AnimationTest, AnimationChannelSampleTest)
{
    AnimChannelData<float> data = {
            {0, 1},
            {3.0f, 4.0f}
    };

    {
        SampleParam param = {AnimFrameTime{0, 0.5}, AnimInterpolation::STEP};
        float val = AnimSampleChannel(data, param);
        ASSERT_FLOAT_EQ(val, 3.0f);
    }

    {
        SampleParam param = {AnimFrameTime{0, 0.5}, AnimInterpolation::LINEAR};
        float val = AnimSampleChannel(data, param);
        ASSERT_FLOAT_EQ(val, 3.5f);
    }

    AnimChannelData<Quaternion> rdata = {
        { 0, 1},
        {
            Quaternion{ float(30.0 / 180.0 * 3.14159265358979323846), Vector3{0.f, 0.f, 1.f}},
            Quaternion{ float(90.0 / 180.0 * 3.14159265358979323846), Vector3{1.f, 0.f, 0.f}}
        }
    };

    {
        SampleParam param = {AnimFrameTime{0, 0.3f}, AnimInterpolation::LINEAR};
        Quaternion val = AnimSampleChannel(rdata, param);
        EXPECT_NEAR(val.x, 0.23f, 0.01f);
        EXPECT_NEAR(val.y, 0.00f, 0.01f);
        EXPECT_NEAR(val.z, 0.19f, 0.01f);
        EXPECT_NEAR(val.w, 0.95f, 0.01f);
    }
}

TEST(AnimationTest, AnimationClipTest)
{

    AnimNodeChannelData data;
    data.name = "T1";
    data.position.times.emplace_back(0);
    data.position.times.emplace_back(1);
    data.position.times.emplace_back(2);
    data.position.times.emplace_back(3);

    data.position.keys.emplace_back(Vector3{0.f, 0.f, 0.f});
    data.position.keys.emplace_back(Vector3{0.f, 0.5f, 0.f});
    data.position.keys.emplace_back(Vector3{0.f, 1.0f, 1.f});
    data.position.keys.emplace_back(Vector3{0.f, 1.0f, 1.f});

    data.position.Compress();

    ASSERT_EQ(data.position.times.size(), 3);

    AnimationClip clip(Name("Test"));

    clip.AddChannel(new AnimationNodeChannel(data));

    clip.SetFrameRate(30.f);
    clip.SetNumFrame(4);

    SkeletonData skeletonData = {};
    skeletonData.boneData.emplace_back(BoneData{Name("T1")});
    skeletonData.refPos.emplace_back(Transform::GetIdentity());

    SkeletonPtr skeleton = Skeleton::BuildSkeleton(skeletonData);

    AnimPose pose;
    pose.transforms.resize(1, Transform::GetIdentity());
    pose.skeleton = skeleton.Get();

    SampleParam param = {};
    param.frameTime = Anim::ConvertFromFrameRate(0.05f, clip.GetPlayRate());
    param.interpolation = AnimInterpolation::LINEAR;

    clip.SamplePose(pose, param);

    ASSERT_NEAR(pose.transforms[0].translation.y, 0.75f, 0.0001f);
    ASSERT_NEAR(pose.transforms[0].translation.z, 0.5f, 0.0001f);
}

TEST(AnimationTest, AnimationParameterTest)
{
    Animation anim(nullptr);

    std::unique_ptr<IAnimParameter> param1(new TAnimFuncParameter<float>([](float time) -> float {
        return time;
    }));

    param1->Update(0.5f);
    ASSERT_FLOAT_EQ(param1->EvalAs<float>(), 0.5f);
}

TEST(AnimationTest, AnimationCompTest)
{
    ASSERT_TRUE(!AnimCompEval<float>::Compare(AnimComp::NEV , 1.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::LT  , 1.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::EQ  , 3.f, 3.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::LE  , 1.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::LE  , 2.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::GT  , 3.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::NE  , 1.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::GE  , 3.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::GE  , 2.f, 2.f));
    ASSERT_TRUE(AnimCompEval<float>::Compare(AnimComp::AWS , 1.f, 2.f));
}

namespace {

    SkeletonPtr MakeTestSkeleton()
    {
        SkeletonData data;
        data.boneData.emplace_back(BoneData{Name("Root")});
        data.refPos.emplace_back(Transform::GetIdentity());
        return Skeleton::BuildSkeleton(data);
    }

    AnimClipPtr MakeTestClip()
    {
        AnimNodeChannelData ch;
        ch.name = "Root";
        ch.position.times = {0, 1, 2, 3};
        ch.position.keys = {
            Vector3(0.f, 0.f, 0.f),
            Vector3(0.f, 1.f, 0.f),
            Vector3(0.f, 2.f, 0.f),
            Vector3(0.f, 3.f, 0.f)
        };

        AnimClipPtr clip = new AnimationClip(Name("TestClip"));
        clip->AddChannel(new AnimationNodeChannel(ch));
        clip->SetFrameRate(1.f);
        return clip;
    }

    class StaticPoseNode : public AnimNode {
    public:
        explicit StaticPoseNode(const Transform& inTrans) : trans(inTrans) {}

        void InitAny(const AnimContext&) override {}
        void EvalAny(AnimationEval& context) override
        {
            if (!context.pose.transforms.empty()) {
                context.pose.transforms[0] = trans;
            }
        }

        Transform trans;
    };

} // namespace

TEST(AnimationTest, AnimationChannelSingleKeyTest)
{
    AnimChannelData<float> data = { {5}, {2.5f} };

    {
        SampleParam param = {AnimFrameTime{0, 0.f}, AnimInterpolation::LINEAR};
        ASSERT_FLOAT_EQ(AnimSampleChannel(data, param), 2.5f);
    }
    {
        SampleParam param = {AnimFrameTime{100, 0.f}, AnimInterpolation::LINEAR};
        ASSERT_FLOAT_EQ(AnimSampleChannel(data, param), 2.5f);
    }
}

TEST(AnimationTest, AnimationChannelResizeTest)
{
    AnimChannelData<Vector3> data;
    data.Resize(4);

    ASSERT_EQ(data.times.size(), 4u);
    ASSERT_EQ(data.keys.size(), 4u);
}

TEST(AnimationTest, AnimationRotationShortestArcTest)
{
    const float rad = 3.14159265358979323846f / 180.f;
    Quaternion q0{20.f * rad, Vector3{0.f, 0.f, 1.f}};
    Quaternion q1{-20.f * rad, Vector3{0.f, 0.f, 1.f}};

    AnimChannelData<Quaternion> data = { {0, 1}, {q0, q1} };

    SampleParam param = {AnimFrameTime{0, 0.5f}, AnimInterpolation::LINEAR};
    Quaternion res = AnimSampleChannel(data, param);

    EXPECT_NEAR(res.Dot(res), 1.f, 1e-4f);
    EXPECT_GT(std::abs(res.w), 0.99f);
}

TEST(AnimationTest, AnimationChannelScaleSampleTest)
{
    AnimNodeChannelData data;
    data.name = "B0";
    data.scale.times = {0, 1};
    data.scale.keys = {Vector3(1.f, 1.f, 1.f), Vector3(3.f, 3.f, 3.f)};

    AnimationNodeChannel channel(data);

    Transform trans = Transform::GetIdentity();
    SampleParam param = {AnimFrameTime{1, 0.f}, AnimInterpolation::LINEAR};
    channel.Sample(param, trans);

    ASSERT_FLOAT_EQ(trans.scale.x, 3.f);
}

TEST(AnimationTest, AnimationChannelAbsentComponentTest)
{
    AnimNodeChannelData data;
    data.name = "B0";
    data.position.times = {0, 1};
    data.position.keys = {Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};

    AnimationNodeChannel channel(data);

    Transform trans = Transform::GetIdentity();
    trans.scale = Vector3(2.f, 2.f, 2.f);
    trans.rotation = Quaternion(3.14159265358979323846f * 0.5f, Vector3{1.f, 0.f, 0.f});

    SampleParam param = {AnimFrameTime{1, 0.f}, AnimInterpolation::LINEAR};
    channel.Sample(param, trans);

    ASSERT_FLOAT_EQ(trans.translation.x, 1.f);
    ASSERT_FLOAT_EQ(trans.scale.x, 2.f);
    ASSERT_FLOAT_EQ(trans.scale.y, 2.f);
    EXPECT_NEAR(trans.rotation.w, std::cos(3.14159265358979323846f * 0.25f), 1e-4f);
}

TEST(AnimationTest, AnimationBoneMaskExcludeTest)
{
    AnimNodeChannelData data;
    data.name = "Root";
    data.position.times = {0, 1};
    data.position.keys = {Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};

    AnimationClip clip(Name("MaskClip"));
    clip.AddChannel(new AnimationNodeChannel(data));

    auto skeleton = MakeTestSkeleton();

    AnimPose pose;
    pose.skeleton = skeleton.Get();
    pose.transforms.resize(1, Transform::GetIdentity());
    pose.boneMask.ResetBit(0);

    SampleParam param = {AnimFrameTime{1, 0.f}, AnimInterpolation::LINEAR};
    clip.SamplePose(pose, param);

    ASSERT_FLOAT_EQ(pose.transforms[0].translation.x, 0.f);
}

TEST(AnimationTest, AnimationPoseResetTest)
{
    AnimPose pose;
    pose.transforms.resize(1, Transform::GetIdentity());
    pose.transforms[0].translation = Vector3(1.f, 0.f, 0.f);
    pose.ResetRefPose();
    ASSERT_FLOAT_EQ(pose.transforms[0].translation.x, 1.f);

    auto skeleton = MakeTestSkeleton();
    AnimPose pose2;
    pose2.skeleton = skeleton.Get();
    pose2.transforms.resize(1, Transform::GetIdentity());
    pose2.transforms[0].translation = Vector3(5.f, 0.f, 0.f);
    pose2.ResetRefPose();
    ASSERT_FLOAT_EQ(pose2.transforms[0].translation.x, 0.f);
}

TEST(AnimationTest, AnimationPoseNormalizeTest)
{
    AnimPose pose;
    pose.transforms.resize(1, Transform::GetIdentity());
    pose.transforms[0].rotation = Quaternion{2.f, 0.f, 0.f, 0.f};

    pose.NormalizeRotation();

    EXPECT_NEAR(pose.transforms[0].rotation.w, 1.f, 1e-5f);
}

TEST(AnimationTest, AnimationPoseSkinMatrixTest)
{
    SkeletonData data;
    data.boneData.emplace_back(BoneData{Name("Root")});
    data.boneData.emplace_back(BoneData{Name("Child"), 0});
    data.refPos.emplace_back(Transform::GetIdentity());
    data.refPos.emplace_back(Transform::GetIdentity());

    auto skeleton = Skeleton::BuildSkeleton(data);

    AnimPose pose;
    pose.skeleton = skeleton.Get();
    pose.transforms.resize(2, Transform::GetIdentity());
    pose.transforms[0].translation = Vector3(2.f, 0.f, 0.f);
    pose.transforms[1].translation = Vector3(3.f, 0.f, 0.f);

    std::vector<Matrix4> skin;
    pose.ToSkinRenderData(skin, Transform::GetIdentity());

    ASSERT_EQ(skin.size(), 2u);
    ASSERT_NEAR(skin[0][3][0], 2.f, 1e-5f);
    ASSERT_NEAR(skin[1][3][0], 5.f, 1e-5f);
}

TEST(AnimationTest, AnimationPoseBlendOverrideTest)
{
    Transform src = Transform::GetIdentity();
    src.translation = Vector3(2.f, 4.f, 6.f);
    src.scale = Vector3(3.f, 3.f, 3.f);

    const Transform base = Transform::GetIdentity();

    Transform d0 = base;
    AnimPose::BlendTransform(src, d0, 0.f);
    ASSERT_FLOAT_EQ(d0.translation.x, 0.f);
    ASSERT_FLOAT_EQ(d0.scale.x, 1.f);

    Transform d5 = base;
    AnimPose::BlendTransform(src, d5, 0.5f);
    ASSERT_FLOAT_EQ(d5.translation.x, 1.f);
    ASSERT_FLOAT_EQ(d5.translation.y, 2.f);
    ASSERT_FLOAT_EQ(d5.scale.x, 2.f);

    Transform d1 = base;
    AnimPose::BlendTransform(src, d1, 1.f);
    ASSERT_FLOAT_EQ(d1.translation.x, 2.f);
    ASSERT_FLOAT_EQ(d1.scale.x, 3.f);
}

TEST(AnimationTest, AnimationPoseBlendAdditiveTest)
{
    Transform delta = Transform::GetIdentity();
    delta.translation = Vector3(2.f, 0.f, 0.f);
    delta.rotation = Quaternion(3.14159265358979323846f * 0.5f, Vector3{0.f, 0.f, 1.f});

    Transform base = Transform::GetIdentity();
    base.translation = Vector3(1.f, 0.f, 0.f);

    Transform d0 = base;
    AnimPose::BlendTransformAdditive(delta, d0, 0.f);
    ASSERT_FLOAT_EQ(d0.translation.x, 1.f);
    EXPECT_NEAR(d0.rotation.w, 1.f, 1e-4f);

    Transform d5 = base;
    AnimPose::BlendTransformAdditive(delta, d5, 0.5f);
    ASSERT_FLOAT_EQ(d5.translation.x, 2.f);
    EXPECT_NEAR(d5.rotation.Dot(d5.rotation), 1.f, 1e-4f);
}

TEST(AnimationTest, AnimationPoseBlendMismatchTest)
{
    AnimPose src;
    src.transforms.resize(2, Transform::GetIdentity());

    AnimPose dst;
    dst.transforms.resize(3, Transform::GetIdentity());
    dst.transforms[0].translation = Vector3(7.f, 0.f, 0.f);

    AnimPose::BlendPose(src, dst, 0.5f, PoseBlendMode::OVERRIDE);

    ASSERT_EQ(dst.transforms.size(), 3u);
    ASSERT_FLOAT_EQ(dst.transforms[0].translation.x, 7.f);
}

TEST(AnimationTest, AnimationPoseBlend2NodeTest)
{
    auto skeleton = MakeTestSkeleton();

    StaticPoseNode nodeA(Transform::GetIdentity());
    Transform b = Transform::GetIdentity();
    b.translation = Vector3(2.f, 0.f, 0.f);
    StaticPoseNode nodeB(b);

    PoseBlend2Node blend(&nodeA, &nodeB);

    AnimContext context{};
    blend.InitAny(context);
    blend.SetBlendTime(1.f);

    blend.SetBlend(false);
    blend.TickAny({}, 0.5f);

    blend.SetBlend(true);
    blend.TickAny({}, 0.5f);

    AnimationEval eval(skeleton);
    blend.EvalAny(eval);

    ASSERT_NEAR(eval.pose.transforms[0].translation.x, 1.f, 1e-4f);
}

TEST(AnimationTest, AnimationPoseBlend2NodeActivationTest)
{
    auto skeleton = MakeTestSkeleton();

    StaticPoseNode nodeA(Transform::GetIdentity());
    Transform b = Transform::GetIdentity();
    b.translation = Vector3(2.f, 0.f, 0.f);
    StaticPoseNode nodeB(b);

    PoseBlend2Node blend(&nodeA, &nodeB);

    AnimContext context{};
    blend.InitAny(context);
    blend.SetBlendTime(1.f);

    blend.SetBlend(false);
    blend.TickAny({}, 0.5f);

    blend.SetBlend(true);
    blend.TickAny({}, 1.f);

    AnimationEval evalOn(skeleton);
    blend.EvalAny(evalOn);
    ASSERT_NEAR(evalOn.pose.transforms[0].translation.x, 2.f, 1e-4f);

    blend.SetBlend(false);
    blend.TickAny({}, 1.f);

    AnimationEval evalOff(skeleton);
    blend.EvalAny(evalOff);
    ASSERT_NEAR(evalOff.pose.transforms[0].translation.x, 0.f, 1e-4f);
}

TEST(AnimationTest, AnimationPoseBlendNodeListTest)
{
    auto skeleton = MakeTestSkeleton();

    StaticPoseNode nodeA(Transform::GetIdentity());
    Transform b = Transform::GetIdentity();
    b.translation = Vector3(4.f, 0.f, 0.f);
    StaticPoseNode nodeB(b);

    {
        PoseBlendNodeList list;
        list.AddPose(&nodeA, 1.f);
        list.AddPose(&nodeB, 0.f);

        AnimationEval eval(skeleton);
        list.EvalAny(eval);
        ASSERT_NEAR(eval.pose.transforms[0].translation.x, 0.f, 1e-4f);
    }

    {
        PoseBlendNodeList list;
        list.AddPose(&nodeA, 1.f);
        list.AddPose(&nodeB, 1.f);

        AnimationEval eval(skeleton);
        list.EvalAny(eval);
        ASSERT_NEAR(eval.pose.transforms[0].translation.x, 2.f, 1e-4f);
    }
}

TEST(AnimationTest, AnimationSequencePlayerRateTest)
{
    AnimClipPtr clip = MakeTestClip();

    AnimationSequencePlayer player;
    player.SetClip(clip);
    player.SetPlaying(true);

    player.SetPlayRate(0.5f);
    player.Tick(1.f);
    ASSERT_NEAR(player.GetCurrentTime(), 0.5f, 1e-5f);

    player.SetPlayRate(-1.f);
    player.Tick(1.f);
    ASSERT_NEAR(player.GetCurrentTime(), 0.f, 1e-5f);
}

TEST(AnimationTest, AnimationSequencePlayerLoopTest)
{
    AnimClipPtr clip = MakeTestClip();

    AnimationSequencePlayer nonLoop;
    nonLoop.SetClip(clip);
    nonLoop.SetLoop(false);
    nonLoop.SetPlaying(true);
    nonLoop.Tick(10.f);
    ASSERT_NEAR(nonLoop.GetCurrentTime(), clip->GetDuration(), 1e-5f);
    ASSERT_FALSE(nonLoop.IsPlaying());

    AnimationSequencePlayer loop;
    loop.SetClip(clip);
    loop.SetLoop(true);
    loop.SetPlaying(true);
    loop.Tick(5.f);
    ASSERT_TRUE(loop.IsPlaying());
    ASSERT_NEAR(loop.GetCurrentTime(), 1.f, 1e-5f);
}

TEST(AnimationTest, AnimationClipDurationTest)
{
    AnimNodeChannelData data;
    data.name = "Root";
    data.position.times = {0, 1, 2, 3};
    data.position.keys = {Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f), Vector3(0.f, 0.f, 0.f)};

    AnimationClip clip(Name("DurationClip"));
    clip.AddChannel(new AnimationNodeChannel(data));
    clip.SetFrameRate(30.f);

    ASSERT_NEAR(clip.GetDuration(), 4.f / 30.f, 1e-5f);

    AnimationClip zeroRateClip(Name("ZeroRateClip"));
    zeroRateClip.AddChannel(new AnimationNodeChannel(data));
    zeroRateClip.SetFrameRate(0.f);

    ASSERT_FLOAT_EQ(zeroRateClip.GetDuration(), 0.f);
}

TEST(AnimationTest, AnimationRootMotionTest)
{
    AnimNodeChannelData data;
    data.name = "Root";
    data.position.times = {0, 1};
    data.position.keys = {Vector3(0.f, 0.f, 0.f), Vector3(1.f, 0.f, 0.f)};

    AnimClipPtr clip = new AnimationClip(Name("RootMotionClip"));
    clip->AddChannel(new AnimationNodeChannel(data));
    clip->SetFrameRate(1.f);

    auto skeleton = MakeTestSkeleton();

    AnimationClipNode::PersistentData persistent;
    persistent.clip = clip;
    persistent.looping = false;
    persistent.rootMotion = false;

    AnimationClipNode node(persistent);
    AnimContext context{};
    node.InitAny(context);
    node.TickAny({}, 1.f);

    AnimationEval eval(skeleton);
    node.EvalAny(eval);

    ASSERT_TRUE(eval.pose.boneMask.CheckBit(0));
    ASSERT_FLOAT_EQ(eval.pose.transforms[0].translation.x, 0.f);
    ASSERT_NEAR(eval.rootMotionDelta.translation.x, 1.f, 1e-4f);

    AnimationClipNode::PersistentData enabledPersistent = persistent;
    enabledPersistent.rootMotion = true;
    AnimationClipNode enabledNode(enabledPersistent);
    enabledNode.InitAny(context);
    enabledNode.TickAny({}, 1.f);

    AnimationEval enabledEval(skeleton);
    enabledNode.EvalAny(enabledEval);
    ASSERT_NEAR(enabledEval.pose.transforms[0].translation.x, 1.f, 1e-4f);
}

TEST(AnimationTest, AnimationEvalDoesNotAdvanceTest)
{
    AnimClipPtr clip = MakeTestClip();
    auto skeleton = MakeTestSkeleton();

    AnimationClipNode::PersistentData persistent;
    persistent.clip = clip;
    persistent.rootMotion = true;

    AnimationClipNode node(persistent);
    AnimContext context{};
    node.InitAny(context);
    node.TickAny({}, 0.5f);

    AnimationEval eval1(skeleton);
    node.EvalAny(eval1);
    const Vector3 t1 = eval1.pose.transforms[0].translation;

    AnimationEval eval2(skeleton);
    node.EvalAny(eval2);
    const Vector3 t2 = eval2.pose.transforms[0].translation;

    ASSERT_FLOAT_EQ(t1.y, t2.y);
    ASSERT_NEAR(t1.y, 0.5f, 1e-4f);
}

TEST(AnimationTest, AnimationParameterTypeTest)
{
    TAnimFuncParameter<bool> boolParam([](float) { return true; });
    boolParam.Update(0.f);
    ASSERT_TRUE(boolParam.EvalAs<bool>());

    TAnimFuncParameter<float> floatParam([](float) { return 2.5f; });
    floatParam.Update(0.f);
    ASSERT_FLOAT_EQ(floatParam.EvalAs<float>(), 2.5f);

    TAnimParameterCond<float> unavailableCond(nullptr, 0.5f, AnimComp::GE);
    ASSERT_FALSE(unavailableCond.Eval());
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
