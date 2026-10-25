//
// ShaderVariantAssembler tests: pipeline bits + vertex semantics + per-shader
// schema (material override / default), spec folding, and budget validation.
//

#include <algorithm>
#include <aurora/pipeline/ShaderVariantAssembler.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    GlobalVariantLayout MakeGlobal(uint16_t reservedBits = 16)
    {
        GlobalVariantLayout global;
        global.reservedBits = reservedBits;
        global.schema.sources.push_back({Name("pipeline"), 0, 1});
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
    inputs.shaderValues = &overrides;

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
    schema.entries.push_back({Name("NUM_LIGHTS"), Name("material"), 1, 4, 0});
    schema.totalBits = 16;

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariantInfo a;
    ASSERT_TRUE(BuildShaderVariant(inputs, a, nullptr));
    EXPECT_TRUE(a.spec.entries.empty());

    ShaderVariant overrides;
    overrides.entries.push_back({Name("NUM_LIGHTS"), 4});
    inputs.shaderValues = &overrides;

    ShaderVariantInfo b;
    ASSERT_TRUE(BuildShaderVariant(inputs, b, nullptr));
    // spec value folded into variantHash (v1)
    EXPECT_NE(a.variantHash, b.variantHash);
    std::vector<ShaderVariantValue> dxil;
    const auto                      combined = MergeVariantSchemas(schema, &global.schema);
    ASSERT_TRUE(CanonicalShaderVariants(&b.values, combined, ShaderTarget::DXIL, dxil));
    const auto lights = std::find_if(dxil.begin(), dxil.end(), [](const auto &value) { return value.name == "NUM_LIGHTS"; });
    ASSERT_NE(lights, dxil.end());
    EXPECT_EQ(lights->value, 4u);
}

TEST(ShaderVariantAssemblerTest, PipelineOwnsPipelineKeys)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariant pipeline;
    pipeline.entries.push_back({Name("SHADOWS"), 1});
    inputs.pipelineValues = &pipeline;

    ShaderVariantInfo info;
    std::string       error;
    ASSERT_TRUE(BuildShaderVariant(inputs, info, &error)) << error;
    EXPECT_NE(global.ToString(info.key).find("SHADOWS=1"), std::string::npos);
}

TEST(ShaderVariantAssemblerTest, MaterialCannotWritePipelineOrVertexKeys)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();
    std::vector<VertexVariantDef> defs = {{Name("HAS_VERTEX_COLOR"), {VertexSemantic::COLOR}}};

    ShaderVariantInputs inputs;
    inputs.global     = &global;
    inputs.schema     = &schema;
    inputs.vertexDefs = &defs;

    ShaderVariant pipelineKey;
    pipelineKey.entries.push_back({Name("SHADOWS"), 1});
    inputs.shaderValues = &pipelineKey;
    ShaderVariantInfo info;
    std::string       error;
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());

    ShaderVariant vertexKey;
    vertexKey.entries.push_back({Name("HAS_VERTEX_COLOR"), 1});
    inputs.shaderValues = &vertexKey;
    error.clear();
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());

    inputs.pipelineValues = &vertexKey;
    inputs.shaderValues = nullptr;
    error.clear();
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ShaderVariantAssemblerTest, RejectsUnknownAndDuplicateValues)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariant unknown;
    unknown.entries.push_back({Name("NOPE"), 1});
    inputs.shaderValues = &unknown;
    ShaderVariantInfo info;
    std::string       error;
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());

    ShaderVariant duplicate;
    duplicate.entries.push_back({Name("USE_SHADOWS"), 0});
    duplicate.entries.push_back({Name("USE_SHADOWS"), 1});
    inputs.shaderValues = &duplicate;
    error.clear();
    EXPECT_FALSE(BuildShaderVariant(inputs, info, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ShaderVariantAssemblerTest, SameMaterialSeparatesVertexAndPipelineInputs)
{
    GlobalVariantLayout global = MakeGlobal();
    ShaderVariantSchema schema = MakePerShader();
    std::vector<VertexVariantDef> defs = {{Name("HAS_VERTEX_COLOR"), {VertexSemantic::COLOR}}};

    ShaderVariant material;
    material.entries.push_back({Name("USE_SHADOWS"), 1});

    ShaderVariantInputs inputs;
    inputs.global         = &global;
    inputs.schema         = &schema;
    inputs.vertexDefs     = &defs;
    inputs.shaderValues = &material;

    ShaderVariantInfo staticInfo;
    ASSERT_TRUE(BuildShaderVariant(inputs, staticInfo, nullptr));

    inputs.vertexMask.Set(VertexSemantic::COLOR, true);
    ShaderVariantInfo colored;
    ASSERT_TRUE(BuildShaderVariant(inputs, colored, nullptr));
    EXPECT_NE(staticInfo.variantHash, colored.variantHash);

    ShaderVariant pipeline;
    pipeline.entries.push_back({Name("SHADOWS"), 1});
    inputs.pipelineValues = &pipeline;
    ShaderVariantInfo shadowedLighting;
    ASSERT_TRUE(BuildShaderVariant(inputs, shadowedLighting, nullptr));
    EXPECT_NE(colored.variantHash, shadowedLighting.variantHash);

    // Cross-region names cannot be smuggled through the material domain.
    ShaderVariant globalName;
    globalName.entries.push_back({Name("SHADOWS"), 0});
    inputs.shaderValues = &globalName;
    ShaderVariantInfo rejected;
    std::string       error;
    EXPECT_FALSE(BuildShaderVariant(inputs, rejected, &error));
    EXPECT_FALSE(error.empty());
}

TEST(ShaderVariantAssemblerTest, PipelineVertexMaterialMatrix)
{
    GlobalVariantLayout           global = MakeGlobal();
    ShaderVariantSchema           schema = MakePerShader();
    std::vector<VertexVariantDef> defs   = {{Name("HAS_VERTEX_COLOR"), {VertexSemantic::COLOR}}};

    std::set<uint64_t> hashes;
    for (uint32_t shadows = 0; shadows != 2; ++shadows) {
        for (uint32_t vertex = 0; vertex != 2; ++vertex) {
            for (uint32_t material = 0; material != 2; ++material) {
                ShaderVariant pipelineValues;
                if (shadows != 0)
                    pipelineValues.entries.push_back({Name("SHADOWS"), 1});
                ShaderVariant shaderValues;
                if (material != 0)
                    shaderValues.entries.push_back({Name("USE_SHADOWS"), 1});

                ShaderVariantInputs inputs;
                inputs.global         = &global;
                inputs.schema         = &schema;
                inputs.vertexDefs     = &defs;
                inputs.pipelineValues = &pipelineValues;
                inputs.shaderValues = &shaderValues;
                if (vertex != 0)
                    inputs.vertexMask.Set(VertexSemantic::COLOR);

                ShaderVariantInfo info;
                std::string       error;
                ASSERT_TRUE(BuildShaderVariant(inputs, info, &error)) << error;
                EXPECT_EQ(info.key.GetPipelineBit(0), shadows != 0);

                const std::string perShader = info.key.ToString(schema, global.ShaderRegionBase());
                EXPECT_NE(perShader.find(std::string("USE_SHADOWS=") + (material ? "1" : "0")), std::string::npos);
                EXPECT_NE(perShader.find(std::string("HAS_VERTEX_COLOR=") + (vertex ? "1" : "0")), std::string::npos);
                hashes.insert(info.variantHash);
            }
        }
    }
    EXPECT_EQ(hashes.size(), 8u);
}

TEST(ShaderVariantAssemblerTest, LargeSelectionsDoNotRequirePackedKeys)
{
    GlobalVariantLayout global = MakeGlobal(120); // P=120
    ShaderVariantSchema schema = MakePerShader(); // S=16 -> 120+16+16 > 128

    ShaderVariantInputs inputs;
    inputs.global = &global;
    inputs.schema = &schema;

    ShaderVariantInfo info;
    std::string       error;
    EXPECT_TRUE(BuildShaderVariant(inputs, info, &error)) << error;
    EXPECT_FALSE(info.hasPackedKey);
    EXPECT_FALSE(info.selection.identity.empty());
}
