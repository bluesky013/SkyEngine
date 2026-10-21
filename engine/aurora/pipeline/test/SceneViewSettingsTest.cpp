#include <aurora/pipeline/SceneRenderFeature.h>
#include <gtest/gtest.h>
#include <limits>

using namespace sky;
using namespace sky::aurora;

TEST(SceneViewSettingsTest, RejectsMalformedAndNonFiniteDisplayInputs)
{
    const Name name("scene");
    auto       feature = CreateSceneRenderFeature(name);
    auto       input   = SceneViewInput(name, {0, 1}, {1, 0});
    EXPECT_FALSE(feature->Validate(input));
    for (float exposure : {-17.f, 17.f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
        EXPECT_TRUE(feature->Validate(SceneViewInput(name, {0, 1}, {1, exposure})));
    EXPECT_TRUE(feature->Validate(SceneViewInput(name, {0, 1}, {2, 0})));
    input.bytes.pop_back();
    EXPECT_TRUE(feature->Validate(input));
    input = SceneViewInput(name, {0, 1}, {});
    input.outputs.clear();
    EXPECT_TRUE(feature->Validate(input));
}
