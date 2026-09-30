//
// ShaderVariantAssembler tests: pipeline bits + vertex semantics + per-shader
// schema (material override / default), spec folding, and budget validation.
//

#include <aurora/pipeline/ShaderVariantAssembler.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    GlobalVariantLayout MakeGlobal(uint16_t reservedBits = 16)
    {
        GlobalVariantLayout global;
        global.reservedBits = reservedBits;
        global.schema.sources.push_back({Name("pipeline"), 0, 16});
        global.schema.entries.push_back({Name("SHADOWS"), Name("pipeline"), 0, 1, 0});
        global.schema.totalBits = 1;
        return global;
    }

    ShaderVariantSchema MakePerShader()
    {
        ShaderVariantSchema schema;
        schema.sources.push_back({Name("material"), 0, 8});
        schema.sources.push_back({Name("vertex"), 8, 8});
        schema.entries.push_back({Name("USE_SHADOWS"), Name("material"), 0, 1, 0});
        schema.entries.push_back({Name("HAS_VERTEX_COLOR"), Name("vertex"), 0, 1, 0});
        schema.totalBits = 16;
        return schema;
    }

} // namespace

TEST(ShaderVariantAssemblerTest, VertexSemantics)
{
    GlobalVariantLayout           global = MakeGlobal();
    ShaderVariantSchema           schema = MakePerShader();
    std::vector<VertexVariantDef> defs   = {
        {Name("HAS_VERTEX_COLOR"), {VertexSemantic::COLOR}},
    };

    ShaderVariantInputs inputs;
    inputs.global     = &global;
    inputs.schema     = &schema;
    inputs.vertexDefs = &defs;
    inputs.vertexMask.Set(VertexSemantic::COLOR, true);

    ShaderVariantInfo info;
    std::string       error;
    ASSERT_TRUE(BuildShaderVariant(inputs, info, &error)) << error;

    // raw V region carries the semantic mask
    EXPECT_TRUE(info.key.GetVertexSemantics(global.VertexRegionBase()).Test(VertexSemantic::COLOR));
    // vertex switch reflected in the per-shader region
    const std::string s = info.key.ToString(schema, global.ShaderRegionBase());
    EXPECT_NE(s.find("HAS_VERTEX_COLOR=1"), std::string::npos);
}

TEST(ShaderVariantAssemblerTest, OverrideVsDefault)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariantInfo defInfo;
    ASSERT_TRUE(BuildShaderVariant(inputs, defInfo, nullptr));
    EXPECT_NE(defInfo.key.ToString(schema, global.ShaderRegionBase()).find("USE_SHADOWS=0"), std::string::npos);

    ShaderVariant overrides;
    overrides.entries.push_back({Name("USE_SHADOWS"), 1});
    inputs.overrides = &overrides;

    ShaderVariantInfo ovInfo;
    ASSERT_TRUE(BuildShaderVariant(inputs, ovInfo, nullptr));
    EXPECT_NE(ovInfo.key.ToString(schema, global.ShaderRegionBase()).find("USE_SHADOWS=1"), std::string::npos);

    // different variant -> different hash
    EXPECT_NE(defInfo.variantHash, ovInfo.variantHash);
}

TEST(ShaderVariantAssemblerTest, SpecFoldedIntoHash)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();
    schema.entries.push_back({Name("NUM_LIGHTS"), Name("material"), 1, 4, 0, true, 7});
    schema.totalBits = 16;

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariantInfo a;
    ASSERT_TRUE(BuildShaderVariant(inputs, a, nullptr));
    ASSERT_EQ(a.spec.entries.size(), 1u);
    EXPECT_EQ(a.spec.entries[0].id, 7u);

    ShaderVariant overrides;
    overrides.entries.push_back({Name("NUM_LIGHTS"), 4});
    inputs.overrides = &overrides;

    ShaderVariantInfo b;
    ASSERT_TRUE(BuildShaderVariant(inputs, b, nullptr));
    // spec value folded into variantHash (v1)
    EXPECT_NE(a.variantHash, b.variantHash);
}

TEST(ShaderVariantAssemblerTest, RegionBudgetExceeded)
{
    GlobalVariantLayout global = MakeGlobal(120); // P=120
    ShaderVariantSchema schema = MakePerShader(); // S=16 -> 120+16+16 > 128

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariantInfo info;
    std::string       error;
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());
}
