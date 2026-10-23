#include <aurora/pipeline/SceneHighlight.h>
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

TEST(SceneViewSettingsTest, HighlightsRejectInvalidStyleIdentityAndPayload)
{
    const Name                   name("scene");
    const auto                   feature = CreateSceneRenderFeature(name);
    std::vector<RenderEntityRef> entities{{{0, 1}, {0, 1}}, {{0, 1}, {1, 1}}};
    EXPECT_FALSE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    EXPECT_FALSE(feature->Validate(SceneHighlightInput(name, {0, 1}, {})));
    SceneHighlightStyle style;
    for (float width : {0.f, 5.f, std::numeric_limits<float>::quiet_NaN()}) {
        style.width = width;
        EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities, style)));
    }
    style            = {};
    style.occluded.w = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities, style)));
    entities.back() = entities.front();
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    entities.back().entity.generation = 0;
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
    auto input = SceneHighlightInput(name, {0, 1}, {});
    input.bytes.pop_back();
    EXPECT_TRUE(feature->Validate(input));
    entities.resize(8193);
    EXPECT_TRUE(feature->Validate(SceneHighlightInput(name, {0, 1}, entities)));
}
