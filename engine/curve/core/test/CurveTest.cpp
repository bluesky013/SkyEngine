#include <curve/Curve.h>
#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

namespace {
    using namespace sky;
    using namespace sky::curve;

    Curve MakeCurve(CurveInterpolation interpolation = CurveInterpolation::LINEAR, CurveWrap wrap = CurveWrap::CLAMP)
    {
        Curve curve;
        curve.channels      = 1;
        curve.interpolation = interpolation;
        curve.preWrap       = wrap;
        curve.postWrap      = wrap;
        curve.keys.push_back({0.f, {0.f, 0.f, 0.f, 0.f}});
        curve.keys.push_back({1.f, {10.f, 0.f, 0.f, 0.f}});
        return curve;
    }
} // namespace

TEST(Curve, ValidatesOrderedKeysAndChannels)
{
    std::string error;
    EXPECT_TRUE(MakeCurve().Validate(error));

    auto curve     = MakeCurve();
    curve.channels = 0;
    EXPECT_FALSE(curve.Validate(error));

    curve              = MakeCurve();
    curve.keys[1].time = 0.f;
    EXPECT_FALSE(curve.Validate(error));

    curve                 = MakeCurve();
    curve.keys[1].value.x = std::numeric_limits<float>::quiet_NaN();
    EXPECT_FALSE(curve.Validate(error));

    curve = MakeCurve();
    curve.keys.clear();
    EXPECT_FALSE(curve.Validate(error));
}

TEST(Curve, LinearInterpolation)
{
    const auto curve = MakeCurve();
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f).x, 5.f);
    EXPECT_FLOAT_EQ(curve.Evaluate(-1.f).x, 0.f);
    EXPECT_FLOAT_EQ(curve.Evaluate(2.f).x, 10.f);
}

TEST(Curve, StepInterpolation)
{
    auto curve         = MakeCurve(CurveInterpolation::STEP);
    curve.keys[1].time = 0.5f;
    EXPECT_FLOAT_EQ(curve.Evaluate(0.49f).x, 0.f);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f).x, 10.f);
}

TEST(Curve, WrapModes)
{
    auto repeat = MakeCurve(CurveInterpolation::LINEAR, CurveWrap::REPEAT);
    EXPECT_NEAR(repeat.Evaluate(1.25f).x, 2.5f, 1e-5f);
    EXPECT_NEAR(repeat.Evaluate(-0.25f).x, 7.5f, 1e-5f);

    auto mirror = MakeCurve(CurveInterpolation::LINEAR, CurveWrap::MIRROR);
    EXPECT_NEAR(mirror.Evaluate(1.25f).x, 7.5f, 1e-5f);
    EXPECT_NEAR(mirror.Evaluate(-0.25f).x, 2.5f, 1e-5f);
}

TEST(Curve, CubicAndLinearExtrapolation)
{
    auto curve               = MakeCurve(CurveInterpolation::CUBIC);
    curve.keys[0].tangentOut = {10.f, 0.f, 0.f, 0.f};
    curve.keys[1].tangentIn  = {10.f, 0.f, 0.f, 0.f};
    EXPECT_NEAR(curve.Evaluate(0.5f).x, 5.f, 1e-4f);

    curve.preWrap  = CurveWrap::LINEAR;
    curve.postWrap = CurveWrap::LINEAR;
    EXPECT_NEAR(curve.Evaluate(-0.25f).x, -2.5f, 1e-4f);
    EXPECT_NEAR(curve.Evaluate(1.25f).x, 12.5f, 1e-4f);
}

TEST(Curve, ZeroesUnusedChannels)
{
    auto curve          = MakeCurve();
    curve.channels      = 2;
    curve.keys[1].value = {10.f, 20.f, 30.f, 40.f};
    const auto value    = curve.Evaluate(0.5f);
    EXPECT_FLOAT_EQ(value.x, 5.f);
    EXPECT_FLOAT_EQ(value.y, 10.f);
    EXPECT_FLOAT_EQ(value.z, 0.f);
    EXPECT_FLOAT_EQ(value.w, 0.f);

    Curve      empty;
    const auto zero = empty.Evaluate(0.5f);
    EXPECT_FLOAT_EQ(zero.x, 0.f);
}

TEST(Curve, ExtrapolationZeroesUnusedChannelsForSegmentsAndSingleKeys)
{
    auto curve          = MakeCurve(CurveInterpolation::LINEAR, CurveWrap::LINEAR);
    curve.keys[0].value = {0.f, 10.f, 20.f, 30.f};
    curve.keys[1].value = {10.f, 20.f, 30.f, 40.f};
    for (const float time : {-0.5f, 0.5f, 1.5f}) {
        const auto value = curve.Evaluate(time);
        EXPECT_FLOAT_EQ(value.x, time * 10.f);
        EXPECT_FLOAT_EQ(value.y, 0.f);
        EXPECT_FLOAT_EQ(value.z, 0.f);
        EXPECT_FLOAT_EQ(value.w, 0.f);
    }
    curve.keys.resize(1);
    for (const float time : {-0.5f, 1.5f}) {
        const auto value = curve.Evaluate(time);
        EXPECT_FLOAT_EQ(value.x, 0.f);
        EXPECT_FLOAT_EQ(value.y, 0.f);
        EXPECT_FLOAT_EQ(value.z, 0.f);
        EXPECT_FLOAT_EQ(value.w, 0.f);
    }
}

TEST(Curve, AvoidsIntermediateOverflowForRepresentableResults)
{
    const float maximum   = std::numeric_limits<float>::max();
    auto        curve     = MakeCurve();
    curve.keys[0].value.x = -maximum;
    curve.keys[1].value.x = maximum;
    std::string error;
    ASSERT_TRUE(curve.Validate(error));
    EXPECT_FLOAT_EQ(curve.Evaluate(0.f).x, -maximum);
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f).x, 0.f);
    EXPECT_FLOAT_EQ(curve.Evaluate(1.f).x, maximum);

    curve.postWrap        = CurveWrap::LINEAR;
    curve.keys[1].value.x = maximum * 0.125f;
    EXPECT_FLOAT_EQ(curve.Evaluate(1.125f).x, maximum * 0.265625f);
    curve.preWrap         = CurveWrap::LINEAR;
    curve.keys[0].value.x = -maximum * 0.125f;
    curve.keys[1].value.x = maximum;
    EXPECT_FLOAT_EQ(curve.Evaluate(-0.125f).x, -maximum * 0.265625f);

    curve.interpolation        = CurveInterpolation::CUBIC;
    curve.keys[0].value.x      = maximum;
    curve.keys[0].tangentOut.x = maximum;
    curve.keys[1].tangentIn.x  = maximum;
    EXPECT_FLOAT_EQ(curve.Evaluate(0.5f).x, maximum);
}
