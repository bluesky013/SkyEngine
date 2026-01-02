//
// Created by Zach Lee on 2025/6/2.
//

#include "animation/core/Skeleton.h"

#include <animation/core/Animation.h>
#include <animation/core/AnimationInterpolation.h>
#include <animation/core/AnimationNodeChannel.h>
#include <animation/core/AnimationUtils.h>
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

    uint32_t val = 0;
    std::unique_ptr<IAnimParameter> param2(new TAnimRefCachedParameter<uint32_t>(val));
    val = 1;


    param1->Update(0.5f);
    param2->Update(0.5f);

    ASSERT_FLOAT_EQ(param1->EvalAs<float>(), 0.5f);
    ASSERT_EQ(param2->EvalAs<uint32_t>(), 1);

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

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
