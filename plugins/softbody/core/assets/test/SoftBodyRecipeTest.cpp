#include <gtest/gtest.h>
#include <softbody/assets/SoftBodyRecipe.h>

#include <limits>

using namespace sky;
using namespace sky::softbody;

namespace {
    SoftBodyRecipe Recipe()
    {
        SoftBodyRecipe recipe;
        recipe.source                               = Uuid::Create();
        recipe.sourceKey                            = "seal";
        recipe.settings.bodyPrimitive               = "seal/body";
        recipe.settings.generator.useExplicitCenter = true;
        recipe.settings.generator.cageCenter        = {0, .1f, 0};
        recipe.controls.push_back({"head", {0, .2f, 0}, phy::SoftBodyAttachmentMode::RootKinematic});
        recipe.settings.attachments.push_back({"seal/eye", "head"});
        return recipe;
    }
} // namespace

TEST(SoftBodyRecipe, RoundTripPreservesControlsMaterialAndExplicitAttachments)
{
    const auto  source = Recipe();
    std::string text;
    ASSERT_TRUE(WriteSoftBodyRecipe(source, text));
    SoftBodyRecipe loaded;
    const auto     status = ParseSoftBodyRecipe(text, loaded);
    ASSERT_TRUE(status) << status.message;
    EXPECT_EQ(loaded.source, source.source);
    EXPECT_EQ(loaded.sourceKey, source.sourceKey);
    ASSERT_EQ(loaded.controls.size(), 1u);
    EXPECT_EQ(loaded.controls[0].mode, phy::SoftBodyAttachmentMode::RootKinematic);
    EXPECT_EQ(loaded.controls[0].position, source.controls[0].position);
    ASSERT_EQ(loaded.settings.attachments.size(), 1u);
    EXPECT_EQ(loaded.settings.attachments[0].controlId, "head");
    EXPECT_EQ(loaded.settings.generator.cageCenter, source.settings.generator.cageCenter);
    EXPECT_FLOAT_EQ(loaded.settings.material.volumeCompliance, source.settings.material.volumeCompliance);
    EXPECT_TRUE(loaded.settings.material.enableRootFollow);
    EXPECT_FLOAT_EQ(loaded.settings.material.followCompliance, source.settings.material.followCompliance);
    std::string rewritten;
    ASSERT_TRUE(WriteSoftBodyRecipe(loaded, rewritten));
    EXPECT_EQ(rewritten, text);
}

TEST(SoftBodyRecipe, DampingIsANonnegativeRateAndInvalidRatesPreserveOutput)
{
    auto source                            = Recipe();
    source.settings.material.linearDamping = 2.f;
    std::string text;
    ASSERT_TRUE(WriteSoftBodyRecipe(source, text));
    SoftBodyRecipe loaded;
    ASSERT_TRUE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_FLOAT_EQ(loaded.settings.material.linearDamping, 2.f);
    for (const float rate : {-1.f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        source.settings.material.linearDamping = rate;
        const auto saved                       = text;
        EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
        EXPECT_EQ(text, saved);
    }
    const auto offset = text.find("\"linearDamping\":2");
    ASSERT_NE(offset, std::string::npos);
    text.replace(offset, std::string("\"linearDamping\":2").size(), "\"linearDamping\":-1");
    EXPECT_FALSE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_FLOAT_EQ(loaded.settings.material.linearDamping, 2.f);
    EXPECT_EQ(loaded.source, source.source);
}

TEST(SoftBodyRecipe, SuppliedTopologyAnchorAndNormalPolicyRoundTripWithoutNarrowingEnums)
{
    auto source                   = Recipe();
    source.settings.normalPolicy  = SoftBodyNormalPolicy::StandardSkinning;
    source.settings.materialSlots = {Uuid::Create(), Uuid::Create(), Uuid::Create()};
    auto &g                       = source.settings.generator;
    g.mode                        = RigGenerationMode::SuppliedCenterFan;
    g.bindingPositionScale        = .7f;
    g.centerIndex                 = 4;
    source.controls               = {{"a", {0, 0, 0}}, {"b", {1, 0, 0}}, {"c", {0, 1, 0}}, {"d", {0, 0, 1}}, {"center", {.2f, .2f, .2f}}};
    g.tetrahedra                  = {{4, 1, 2, 3}, {4, 0, 3, 2}, {4, 0, 1, 3}, {4, 0, 2, 1}};
    source.settings.attachments   = {{"seal/eye", "", AttachmentBindingMode::BindingAnchor, {.1f, .2f, .3f}}};
    std::string text;
    ASSERT_TRUE(WriteSoftBodyRecipe(source, text));
    SoftBodyRecipe loaded;
    ASSERT_TRUE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_EQ(loaded.version, 2u);
    EXPECT_EQ(loaded.settings.generator.mode, RigGenerationMode::SuppliedCenterFan);
    EXPECT_EQ(loaded.settings.generator.centerIndex, 4u);
    ASSERT_EQ(loaded.settings.generator.tetrahedra.size(), 4u);
    EXPECT_EQ(loaded.settings.generator.tetrahedra[2].d, 3u);
    EXPECT_FLOAT_EQ(loaded.settings.generator.bindingPositionScale, .7f);
    EXPECT_EQ(loaded.settings.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
    EXPECT_EQ(loaded.settings.materialSlots, source.settings.materialSlots);
    ASSERT_EQ(loaded.settings.attachments.size(), 1u);
    EXPECT_EQ(loaded.settings.attachments[0].mode, AttachmentBindingMode::BindingAnchor);
    EXPECT_EQ(loaded.settings.attachments[0].bindingAnchor, source.settings.attachments[0].bindingAnchor);
    for (const auto key : {std::string("\"mode\":1"), std::string("\"normalPolicy\":1")}) {
        const auto offset = text.find(key);
        ASSERT_NE(offset, std::string::npos);
        for (const auto value : {"2", "256", "4294967295", "-1"}) {
            auto bad = text;
            bad.replace(offset + key.size() - 1, 1, value);
            EXPECT_FALSE(ParseSoftBodyRecipe(bad, loaded));
            EXPECT_EQ(loaded.source, source.source);
            EXPECT_EQ(loaded.settings.normalPolicy, SoftBodyNormalPolicy::StandardSkinning);
        }
    }
    source.version = 1;
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    auto legacy    = Recipe();
    legacy.version = 1;
    ASSERT_TRUE(WriteSoftBodyRecipe(legacy, text));
    ASSERT_TRUE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_EQ(loaded.settings.generator.mode, RigGenerationMode::AutoFit);
    EXPECT_FLOAT_EQ(loaded.settings.generator.bindingPositionScale, 1.f);
    EXPECT_EQ(loaded.settings.normalPolicy, SoftBodyNormalPolicy::TetGradient);
}

TEST(SoftBodyRecipe, RawEndAndWideInvalidModesCannotNarrowOrReplaceValidRecipe)
{
    const auto  source = Recipe();
    std::string text;
    ASSERT_TRUE(WriteSoftBodyRecipe(source, text));
    const auto offset = text.find("\"mode\":2");
    ASSERT_NE(offset, std::string::npos);
    auto loaded = source;
    for (const auto value : {"3", "258", "4294967295", "-1"}) {
        auto bad = text;
        bad.replace(offset, 8, std::string("\"mode\":") + value);
        EXPECT_FALSE(ParseSoftBodyRecipe(bad, loaded));
        EXPECT_EQ(loaded.source, source.source);
        EXPECT_EQ(loaded.controls[0].mode, source.controls[0].mode);
    }
}

TEST(SoftBodyRecipe, InvalidControlAndAttachmentReferencesPreserveSerializedOutput)
{
    auto        source = Recipe();
    std::string text   = "previous";
    source.controls.push_back(source.controls.front());
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    EXPECT_EQ(text, "previous");
    source.controls.pop_back();
    source.settings.attachments[0].controlId = "missing";
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    EXPECT_EQ(text, "previous");
    source                                = Recipe();
    source.settings.material.particleMass = 0;
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    EXPECT_EQ(text, "previous");
}

TEST(SoftBodyRecipe, ConfigurableBudgetAndFiniteCompliantFollowRespectRuntimeCapacity)
{
    auto        source                         = Recipe();
    std::string text                           = "previous";
    source.settings.generator.maxControlPoints = MAX_RIG_CONTROL_POINTS + 1;
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    EXPECT_EQ(text, "previous");
    source.settings.generator.maxControlPoints = 48;
    source.settings.material.followCompliance  = 0.f;
    EXPECT_FALSE(WriteSoftBodyRecipe(source, text));
    source.settings.material.followCompliance = .02f;
    EXPECT_TRUE(WriteSoftBodyRecipe(source, text));
    SoftBodyRecipe loaded;
    ASSERT_TRUE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_EQ(loaded.settings.generator.maxControlPoints, 48u);
}

TEST(SoftBodyRecipe, ExactBudgetRoundTripsAndLegacyRecipesDefaultToAnUpperBound)
{
    auto source                                 = Recipe();
    source.settings.generator.maxControlPoints  = 10;
    source.settings.generator.fillControlBudget = true;
    std::string text;
    ASSERT_TRUE(WriteSoftBodyRecipe(source, text));
    SoftBodyRecipe loaded;
    ASSERT_TRUE(ParseSoftBodyRecipe(text, loaded));
    EXPECT_TRUE(loaded.settings.generator.fillControlBudget);
    EXPECT_EQ(loaded.settings.generator.maxControlPoints, 10u);
    const std::string field  = "\"fillControlBudget\":true,";
    const auto        offset = text.find(field);
    ASSERT_NE(offset, std::string::npos);
    auto legacy = text;
    legacy.erase(offset, field.size());
    ASSERT_TRUE(ParseSoftBodyRecipe(legacy, loaded));
    EXPECT_FALSE(loaded.settings.generator.fillControlBudget);
    auto invalid = text;
    invalid.replace(offset, field.size(), "\"fillControlBudget\":1,");
    EXPECT_FALSE(ParseSoftBodyRecipe(invalid, loaded));
    EXPECT_FALSE(loaded.settings.generator.fillControlBudget);
    EXPECT_EQ(loaded.source, source.source);
}
