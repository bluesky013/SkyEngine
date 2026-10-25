//
// GlobalVariantLayout tests: data-driven pipeline variant layout load + validate.
//

#include <aurora/pipeline/GlobalVariantLayout.h>
#include <aurora/pipeline/ShaderVariantAssembler.h>
#include <aurora/shader/ShaderCacheStore.h>
#include <aurora/shader/ShaderCompanion.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <filesystem>

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

TEST(GlobalVariantLayoutTest, PublishedSelectionRetainsAllThreeOwnersWithoutSources)
{
    GlobalVariantLayout layout;
    std::string         error;
    ASSERT_TRUE(LoadLayout(kPipelineVariantSource, layout, error));
    ShaderCompanion companion;
    ASSERT_TRUE(ParseShaderCompanion(R"({
        "sources":[{"name":"local","width":3}],
        "variants":[{"key":"COLOR_ON","source":"local"},{"key":"INSTANCED","source":"local"},{"key":"TEXTURED","source":"local"}],
        "vertex":[{"name":"COLOR_ON","semantics":["COLOR"]},{"name":"INSTANCED","semantics":["INSTANCE"]}]
    })",
                                     companion, &error))
        << error;
    const auto       schema = MergeVariantSchemas(companion.schema, &layout.schema);
    ShaderCacheIndex index;
    index.paths.push_back({"test/Surface.slang", {HashSchema(schema), schema, {}}});
    const auto path = std::filesystem::temp_directory_path() / ("aurora-selection-" + std::to_string(::rand()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } cleanup{path};
    std::filesystem::create_directories(path);
    NativeFileSystem products{FilePath(path.string())};
    ASSERT_TRUE(SaveIndex(products, ShaderTargetDirName(0), index));
    GlobalVariantLayout published;
    ASSERT_TRUE(published.LoadPublished(products, &error)) << error;
    ShaderCacheIndex decoded;
    ASSERT_TRUE(LoadIndex(products, ShaderTargetDirName(0), decoded));
    const auto         local = SelectVariantDomain(decoded.paths.front().second.schema, false);
    ShaderVariant      pipeline{{{Name("SHADOWS"), 1}}};
    ShaderVariant      material{{{Name("TEXTURED"), 1}}};
    VertexSemanticMask mask;
    mask.Set(VertexSemantic::COLOR);
    mask.Set(VertexSemantic::INSTANCE);
    ShaderVariantInputs inputs{&published, &local, nullptr, mask, &pipeline, &material};
    ShaderVariantInfo   resolved;
    ASSERT_TRUE(BuildShaderVariant(inputs, resolved, &error)) << error;
    EXPECT_NE(resolved.values.ToString().find("SHADOWS=1"), std::string::npos);
    EXPECT_NE(resolved.values.ToString().find("INSTANCED=1"), std::string::npos);
    EXPECT_NE(resolved.values.ToString().find("COLOR_ON=1"), std::string::npos);
    material.entries.push_back({Name("INSTANCED"), 0});
    EXPECT_FALSE(BuildShaderVariant(inputs, resolved, &error));
}

TEST(GlobalVariantLayoutTest, ToStringWithPerShader)
{
    GlobalVariantLayout layout;
    std::string error;
    ASSERT_TRUE(LoadLayout(kPipelineVariantSource, layout, error)) << error;

    // per-shader schema (relative offsets)
    ShaderVariantSchema perShader;
    perShader.sources.push_back({Name("vertex"), 0, 1});
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
