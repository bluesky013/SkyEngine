//
// GlobalVariantLayout tests: data-driven pipeline variant layout load + validate.
//

#include <aurora/pipeline/GlobalVariantLayout.h>
#include <aurora/shader/ShaderFileSystem.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kPipelineVariantSource = R"(
// ===== @variant =====
// @reserved 16
// @source pipeline
//   SHADOWS : bool = 0
//   IBL     : bool = 0
// ===================
)";

    bool LoadLayout(const char *source, GlobalVariantLayout &layout, std::string &error)
    {
        ShaderFileSystem fs;
        fs.AddVirtualFile("pipeline_variants.slang", source);
        return layout.Load(fs, "pipeline_variants.slang", &error);
    }

} // namespace

TEST(GlobalVariantLayoutTest, LoadAndValidate)
{
    GlobalVariantLayout layout;
    std::string error;
    ASSERT_TRUE(LoadLayout(kPipelineVariantSource, layout, error)) << error;

    EXPECT_EQ(layout.reservedBits, 16u);
    EXPECT_EQ(layout.schema.totalBits, 2u);
    ASSERT_NE(layout.Find(Name("SHADOWS")), nullptr);
    ASSERT_NE(layout.Find(Name("IBL")), nullptr);

    ShaderVariantKey key;
    key.Set(layout.schema, Name("SHADOWS"), 1);
    EXPECT_NE(layout.ToString(key).find("SHADOWS=1"), std::string::npos);
    EXPECT_NE(layout.ToString(key).find("IBL=0"), std::string::npos);
}

TEST(GlobalVariantLayoutTest, ToStringWithPerShader)
{
    GlobalVariantLayout layout;
    std::string error;
    ASSERT_TRUE(LoadLayout(kPipelineVariantSource, layout, error)) << error;

    // per-shader schema (relative offsets)
    ShaderVariantSchema perShader;
    perShader.sources.push_back({Name("vertex"), 0, 8});
    perShader.entries.push_back({Name("HAS_SKIN"), Name("vertex"), 0, 1, 0});
    perShader.totalBits = 1;

    // compose: pipeline bits [0..reservedBits) + per-shader bits [reservedBits..)
    ShaderVariantKey key;
    key.Set(layout.schema, Name("SHADOWS"), 1);   // pipeline bit 0

    ShaderVariantKey per;
    per.Set(perShader, Name("HAS_SKIN"), 1);       // per-shader bit 0
    per <<= layout.reservedBits;                   // shift to bit 16
    key |= per;

    const std::string s = layout.ToString(key, perShader);
    EXPECT_NE(s.find("SHADOWS=1"), std::string::npos);
    EXPECT_NE(s.find("IBL=0"), std::string::npos);
    EXPECT_NE(s.find("HAS_SKIN=1"), std::string::npos);
}

TEST(GlobalVariantLayoutTest, ExceedsReservedBitsRejected)
{
    const char *source = R"(
// ===== @variant =====
// @reserved 2
// @source pipeline
//   SHADOWS : bool = 0
//   IBL     : bool = 0
//   MSAA    : bool = 0
// ===================
)";

    GlobalVariantLayout layout;
    std::string error;
    EXPECT_FALSE(LoadLayout(source, layout, error));
}
