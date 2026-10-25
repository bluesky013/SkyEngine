//
// ShaderCompanion tests: `.slang.json` schema parsing + resolution.
//

#include <aurora/shader/ShaderCompanion.h>
#include <aurora/shader/ShaderFileSystem.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kCompanion = R"({
        "sources": [
            {"name": "material", "width": 8},
            {"name": "vertex",   "width": 8}
        ],
        "variants": [
            {"key": "USE_SHADOWS", "source": "material", "width": 1, "default": 0},
            {"key": "NUM_LIGHTS",  "source": "material", "width": 4, "symbol": "kNumLights"},
            {"key": "HAS_VERTEX_COLOR", "source": "vertex", "width": 1}
        ],
        "vertex": [
            {"name": "HAS_VERTEX_COLOR", "semantics": ["COLOR"]}
        ],
        "entries": ["VSMain", "FSMain"],
        "depends": ["common.slang"]
    })";

} // namespace

TEST(ShaderCompanionTest, Parse)
{
    ShaderCompanion c;
    std::string     error;
    ASSERT_TRUE(ParseShaderCompanion(kCompanion, c, &error)) << error;

    ASSERT_EQ(c.schema.sources.size(), 2u);
    EXPECT_EQ(c.schema.sources[0].name, Name("material"));
    EXPECT_EQ(c.schema.sources[0].bitOffset, 0);
    EXPECT_EQ(c.schema.sources[1].bitOffset, 8);
    EXPECT_EQ(c.schema.totalBits, 16u);

    ASSERT_EQ(c.schema.entries.size(), 3u);
    EXPECT_EQ(c.schema.entries[2].domain, ShaderVariantDomain::VERTEX);
    EXPECT_EQ(c.schema.entries[2].vertexSemantics, 1u << static_cast<uint8_t>(VertexSemantic::COLOR));
    EXPECT_EQ(c.schema.entries[0].key, Name("USE_SHADOWS"));
    EXPECT_EQ(c.schema.entries[0].bitOffset, 0);
    EXPECT_EQ(c.schema.entries[0].bitWidth, 1);
    EXPECT_EQ(c.schema.entries[1].bitOffset, 1);
    EXPECT_EQ(c.schema.entries[1].symbol, Name("kNumLights"));

    ASSERT_EQ(c.vertexDefs.size(), 1u);
    EXPECT_EQ(c.vertexDefs[0].name, Name("HAS_VERTEX_COLOR"));
    ASSERT_EQ(c.vertexDefs[0].semantics.size(), 1u);
    EXPECT_EQ(c.vertexDefs[0].semantics[0], VertexSemantic::COLOR);

    EXPECT_EQ(c.entryPoints.size(), 2u);
    EXPECT_EQ(c.depends.size(), 1u);
    EXPECT_NE(c.fingerprint, 0u);
}

TEST(ShaderCompanionTest, LoadViaFileSystem)
{
    ShaderFileSystem fs;
    fs.AddVirtualFile("material/lit.slang.json", kCompanion);

    ShaderCompanion c;
    std::string     error;
    ASSERT_TRUE(LoadShaderCompanion(fs, "material/lit.slang", c, &error)) << error;
    EXPECT_EQ(c.schema.totalBits, 16u);

    ShaderCompanion missing;
    EXPECT_FALSE(LoadShaderCompanion(fs, "material/missing.slang", missing, &error));
}

TEST(ShaderCompanionTest, InvalidRejected)
{
    ShaderCompanion c;
    std::string     error;
    EXPECT_FALSE(ParseShaderCompanion("{ not json", c, &error));

    const char *badSource = R"({
        "sources": [{"name": "material", "width": 8}],
        "variants": [{"key": "X", "source": "nope", "width": 1}]
    })";
    EXPECT_FALSE(ParseShaderCompanion(badSource, c, &error));
}

TEST(ShaderCompanionTest, CoreSelectionNormalizesTypedDomainsAndRejectsWrongOwners)
{
    ShaderVariantSchema schema;
    schema.sources           = {{Name("all"), 0, 10}};
    schema.totalBits         = 10;
    schema.entries           = {{Name("P"), Name("all"), 0, 1, 0}, {Name("V"), Name("all"), 1, 1, 0}, {Name("S"), Name("all"), 2, 8, uint32_t(-3)}};
    schema.entries[0].domain = ShaderVariantDomain::PIPELINE;
    schema.entries[0].type   = ShaderVariantType::BOOL;
    schema.entries[1].domain = ShaderVariantDomain::VERTEX;
    schema.entries[1].vertexSemantics = 1u << static_cast<uint8_t>(VertexSemantic::COLOR);
    schema.entries[2].type            = ShaderVariantType::INT32;
    ShaderVariantSelection implicit, explicitDefaults;
    std::string            error;
    ASSERT_TRUE(ResolveShaderVariant(schema, {}, nullptr, nullptr, implicit, error)) << error;
    ShaderVariant pipeline, shader;
    pipeline.entries = {{Name("P"), 0, ShaderVariantType::BOOL}};
    shader.entries   = {{Name("S"), uint32_t(-3), ShaderVariantType::INT32}};
    ASSERT_TRUE(ResolveShaderVariant(schema, {}, &pipeline, &shader, explicitDefaults, error)) << error;
    EXPECT_EQ(implicit.identity, explicitDefaults.identity);
    EXPECT_EQ(implicit.fingerprint, explicitDefaults.fingerprint);
    shader.entries[0].value = 128;
    EXPECT_FALSE(ResolveShaderVariant(schema, {}, &pipeline, &shader, explicitDefaults, error));
    shader.entries[0].value = 1;
    shader.entries[0].type  = ShaderVariantType::UINT32;
    EXPECT_FALSE(ResolveShaderVariant(schema, {}, &pipeline, &shader, explicitDefaults, error));
    shader.entries = {{Name("P"), 1, ShaderVariantType::BOOL}};
    EXPECT_FALSE(ResolveShaderVariant(schema, {}, nullptr, &shader, explicitDefaults, error));
    shader.entries = {{Name("V"), 1}};
    EXPECT_FALSE(ResolveShaderVariant(schema, {}, nullptr, &shader, explicitDefaults, error));
    shader.entries = {{Name("S"), 1, ShaderVariantType::INT32}, {Name("S"), 1, ShaderVariantType::INT32}};
    EXPECT_FALSE(ResolveShaderVariant(schema, {}, nullptr, &shader, explicitDefaults, error));
    EXPECT_EQ(implicit.identity, explicitDefaults.identity);
}

TEST(ShaderCompanionTest, TypedSourceAssociationsAndMalformedJsonAreTransactional)
{
    ShaderCompanion companion;
    std::string     error;
    ASSERT_TRUE(ParseShaderCompanion(R"({"sources":[{"name":"shader","width":9}],"variants":[
        {"key":"ENABLED","source":"shader","type":"bool","default":true},
        {"key":"COUNT","source":"shader","type":"int32","width":8,"default":-2,"symbol":"kCount"}]})",
                                     companion, &error))
        << error;
    EXPECT_EQ(companion.schema.entries[0].type, ShaderVariantType::BOOL);
    EXPECT_EQ(companion.schema.entries[1].defaultValue, uint32_t(-2));
    EXPECT_EQ(companion.schema.entries[1].symbol, Name("kCount"));
    const auto fingerprint = companion.fingerprint;
    for (const char *invalid : {R"({"variants":[],"variants":[]})", R"({"sources":[{"name":"x","width":32769}]})",
                                R"({"sources":[{"name":"x","width":8}],"variants":[{"key":"A","source":"x","type":"bool","default":1}]})",
                                R"({"sources":[{"name":"x","width":8}],"variants":[{"key":"A","source":"x","spec":1}]})"}) {
        EXPECT_FALSE(ParseShaderCompanion(invalid, companion, &error));
        EXPECT_EQ(companion.fingerprint, fingerprint);
    }
}

TEST(ShaderCompanionTest, LogicalSelectionSupportsMoreThanThePackedKeyBudget)
{
    ShaderVariantSchema schema;
    schema.totalBits = 160;
    schema.sources   = {{Name("shader"), 0, 160}};
    for (uint16_t index = 0; index < 5; ++index)
        schema.entries.push_back({Name(("VALUE" + std::to_string(index)).c_str()), Name("shader"), uint16_t(index * 32), 32, index});
    ShaderVariantSelection selected;
    std::string            error;
    ASSERT_TRUE(ResolveShaderVariant(schema, {}, nullptr, nullptr, selected, error)) << error;
    ASSERT_EQ(selected.identity.size(), 5u);
    EXPECT_EQ(selected.identity.back().value, 4u);
    ShaderVariantKey packed;
    EXPECT_FALSE(packed.Set(schema, Name("VALUE4"), 4));
    EXPECT_FALSE(packed.Get(schema, Name("VALUE4")));
    EXPECT_EQ(packed.totalBits, 0u);
    EXPECT_EQ(packed.words[0], 0u);
    EXPECT_EQ(packed.words[1], 0u);
}

TEST(ShaderCompanionTest, PipelineOwnershipPreservesStageSourceAssociations)
{
    ShaderVariantSchema pipeline;
    pipeline.sources           = {{Name("pipeline"), 0, 1}};
    pipeline.entries           = {{Name("SHADOWS"), Name("pipeline"), 0, 1, 0}};
    pipeline.entries[0].domain = ShaderVariantDomain::PIPELINE;
    pipeline.totalBits         = 1;
    auto stage                 = pipeline;
    stage.entries[0].symbol    = Name("kShadows");
    const auto merged          = MergeVariantSchemas(stage, &pipeline);
    ASSERT_TRUE(merged.Validate());
    ASSERT_EQ(merged.entries.size(), 1u);
    EXPECT_EQ(merged.entries[0].domain, ShaderVariantDomain::PIPELINE);
    EXPECT_EQ(merged.entries[0].symbol, Name("kShadows"));
}

TEST(ShaderCompanionTest, SourceBoundsAndDuplicateNamesAreRejected)
{
    ShaderVariantSchema schema;
    schema.sources   = {{Name("source"), 0, 1}};
    schema.entries   = {{Name("VALUE"), Name("source"), 1, 1, 0}};
    schema.totalBits = 2;
    EXPECT_FALSE(schema.Validate());
    schema.entries[0].bitOffset = 0;
    ASSERT_TRUE(schema.Validate());
    schema.sources.push_back(schema.sources[0]);
    EXPECT_FALSE(schema.Validate());
    schema.sources.pop_back();
    schema.sources[0].bitWidth = 3;
    EXPECT_FALSE(schema.Validate());
}
