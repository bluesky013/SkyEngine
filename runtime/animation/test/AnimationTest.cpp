//
// Created by Zach Lee on 2025/6/2.
//

#include <gtest/gtest.h>
#include <animation/core/AnimationInterpolation.h>
#include <animation/core/Animation.h>
using namespace sky;

TEST(AnimationTest, AnimationChannelDataTest)
{
    AnimChannelData<float> data = {
            {0.f, 0.5f, 1.5f, 2.5f},
            {3.0f, 4.0f, 5.0f, 6.0f}
    };

    auto k1 = data.FindKeyFrame(-1.f);
    ASSERT_EQ(k1.first, 0);
    ASSERT_EQ(k1.second, 0);

    auto k2 = data.FindKeyFrame(3.f);
    ASSERT_EQ(k2.first, 3);
    ASSERT_EQ(k2.second, 3);

    auto k3 = data.FindKeyFrame(1.f);
    ASSERT_EQ(k3.first, 1);
    ASSERT_EQ(k3.second, 2);

    auto k4 = data.FindKeyFrame(2.5f);
    ASSERT_EQ(k4.first, 3);
    ASSERT_EQ(k4.second, 3);

    auto k5 = data.FindKeyFrame(0.f);
    ASSERT_EQ(k5.first, 0);
    ASSERT_EQ(k5.second, 1);

    auto k6 = data.FindKeyFrame(0.1f);
    ASSERT_EQ(k6.first, 0);
    ASSERT_EQ(k6.second, 1);
}

TEST(AnimationTest, AnimationChannelSampleTest)
{
    AnimChannelData<float> data = {
            {0.f, 0.5f},
            {3.0f, 4.0f}
    };

    {
        SampleParam param = {0.25f, AnimInterpolation::STEP};
        float val = AnimSampleChannel(data, param);
        ASSERT_FLOAT_EQ(val, 3.0f);
    }

    {
        SampleParam param = {0.25f, AnimInterpolation::LINEAR};
        float val = AnimSampleChannel(data, param);
        ASSERT_FLOAT_EQ(val, 3.5f);
    }

    AnimChannelData<Quaternion> rdata = {
        { 0.f, 1.f},
        {
            Quaternion{ float(30.0 / 180.0 * 3.14159265358979323846), Vector3{0.f, 0.f, 1.f}},
            Quaternion{ float(90.0 / 180.0 * 3.14159265358979323846), Vector3{1.f, 0.f, 0.f}}
        }
    };

    {
        SampleParam param = {0.3f, AnimInterpolation::LINEAR};
        Quaternion val = AnimSampleChannel(rdata, param);
        EXPECT_NEAR(val.x, 0.23f, 0.01f);
        EXPECT_NEAR(val.y, 0.00f, 0.01f);
        EXPECT_NEAR(val.z, 0.19f, 0.01f);
        EXPECT_NEAR(val.w, 0.95f, 0.01f);
    }
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
