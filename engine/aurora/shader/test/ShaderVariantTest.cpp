//
// ShaderVariant tests: bitmask key, schema validation, and unified hash.
//

#include <aurora/rhi/Shader.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderVariant.h>
#include <aurora/shader/gen/ShaderVariantGen.h>
#include <core/platform/Platform.h>

#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {

    ShaderVariantSchema MakeSchema()
    {
        ShaderVariantSchema s;
        s.sources.push_back({Name("vertex"), 16, 8});
        s.sources.push_back({Name("batch"), 24, 40});
        s.entries.push_back({Name("HAS_SKIN"), Name("vertex"), 0, 1, 0});
        s.entries.push_back({Name("HAS_VERTEX_COLOR"), Name("vertex"), 1, 1, 0});
        s.entries.push_back({Name("HAS_EMISSIVE"), Name("batch"), 0, 1, 0});
        s.entries.push_back({Name("NUM_LIGHTS"), Name("batch"), 1, 4, 4});
        s.totalBits = 64;
        return s;
    }

} // namespace

TEST(ShaderVariantTest, ContentHashOrderInsensitive)
{
    ShaderVariant a;
    a.entries.push_back({Name("A"), 1});
    a.entries.push_back({Name("B"), 2});

    ShaderVariant b;
    b.entries.push_back({Name("B"), 2});
    b.entries.push_back({Name("A"), 1});

    EXPECT_EQ(a.ContentHash(), b.ContentHash());
}

TEST(ShaderVariantTest, ContentHashDiffersByValue)
{
    ShaderVariant a;
    a.entries.push_back({Name("A"), 1});

    ShaderVariant b;
    b.entries.push_back({Name("A"), 2});

    EXPECT_NE(a.ContentHash(), b.ContentHash());
}

TEST(ShaderVariantTest, KeySetGet)
{
    auto schema = MakeSchema();

    ShaderVariantKey key;
    key.Set(schema, Name("HAS_SKIN"), 1);
    key.Set(schema, Name("NUM_LIGHTS"), 5);

    EXPECT_EQ(key.Get(schema, Name("HAS_SKIN")), 1u);
    EXPECT_EQ(key.Get(schema, Name("NUM_LIGHTS")), 5u);
    EXPECT_EQ(key.Get(schema, Name("HAS_VERTEX_COLOR")), 0u);
}

TEST(ShaderVariantTest, KeyOrCombine)
{
    auto schema = MakeSchema();

    ShaderVariantKey a;
    a.Set(schema, Name("HAS_SKIN"), 1);
    ShaderVariantKey b;
    b.Set(schema, Name("HAS_EMISSIVE"), 1);

    a |= b;
    EXPECT_EQ(a.Get(schema, Name("HAS_SKIN")), 1u);
    EXPECT_EQ(a.Get(schema, Name("HAS_EMISSIVE")), 1u);
}

TEST(ShaderVariantTest, PipelineBit)
{
    ShaderVariantKey key;
    key.SetPipelineBit(0, true);
    key.SetPipelineBit(1, false);

    EXPECT_EQ(key.GetPipelineBit(0), 1u);
    EXPECT_EQ(key.GetPipelineBit(1), 0u);
}

TEST(ShaderVariantTest, KeyShiftLeft)
{
    ShaderVariantSchema s;
    s.sources.push_back({Name("batch"), 0, 1});
    s.entries.push_back({Name("HAS_EMISSIVE"), Name("batch"), 0, 1, 0});
    s.totalBits = 1;

    ShaderVariantKey key;
    ASSERT_TRUE(key.Set(s, Name("HAS_EMISSIVE"), 1));
    key <<= 16;                          // shift to bit 16

    EXPECT_EQ(key.GetPipelineBit(16), 1u);
    EXPECT_EQ(key.GetPipelineBit(0), 0u);
}

TEST(ShaderVariantTest, SchemaOverlapRejected)
{
    ShaderVariantSchema s;
    s.sources.push_back({Name("vertex"), 16, 8});
    s.entries.push_back({Name("A"), Name("vertex"), 0, 1, 0});
    s.entries.push_back({Name("B"), Name("vertex"), 0, 1, 0}); // overlaps A
    s.totalBits = 24;

    std::string error;
    EXPECT_FALSE(s.Validate(&error));
}

TEST(ShaderVariantTest, SchemaTooManyBitsRejected)
{
    ShaderVariantSchema s;
    s.totalBits = ShaderVariantSchema::kMaxLogicalBits + 1;

    std::string error;
    EXPECT_FALSE(s.Validate(&error));
}

TEST(ShaderVariantTest, SchemaValid)
{
    auto schema = MakeSchema();
    EXPECT_TRUE(schema.Validate());
}

TEST(ShaderVariantTest, MacroVariantCompilesDifferentPermutation)
{
    const char *source = R"(
[shader("fragment")]
float4 mainFS() : SV_Target
{
#if USE_SHADOWS
    return float4(1.0, 0.0, 0.0, 1.0);
#else
    return float4(0.0, 0.0, 1.0, 1.0);
#endif
}
)";

    ShaderCompilerSlang compiler;

    ShaderVariant on;
    on.entries.push_back({Name("USE_SHADOWS"), 1});
    ShaderCompileDesc descOn{};
    descOn.source  = source;
    descOn.entry   = "mainFS";
    descOn.stage   = ShaderStageFlagBit::FS;
    descOn.target  = ShaderTarget::SPIRV;
    descOn.variant = &on;
    ShaderCompileResult resultOn{};
    ASSERT_TRUE(compiler.Compile(descOn, resultOn)) << resultOn.errorInfo;

    ShaderVariant off;
    off.entries.push_back({Name("USE_SHADOWS"), 0});
    ShaderCompileDesc descOff{};
    descOff.source  = source;
    descOff.entry   = "mainFS";
    descOff.stage   = ShaderStageFlagBit::FS;
    descOff.target  = ShaderTarget::SPIRV;
    descOff.variant = &off;
    ShaderCompileResult resultOff{};
    ASSERT_TRUE(compiler.Compile(descOff, resultOff)) << resultOff.errorInfo;

    EXPECT_NE(resultOn.data, resultOff.data);
}

namespace {

    const char *kSpecShader = R"(
#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif

#if AURORA_TARGET_DXIL
    static const uint kNumLights = NUM_LIGHTS;
#else
    [SpecializationConstant]
    [[vk::constant_id(0)]]
    const uint kNumLights = NUM_LIGHTS;
#endif

[shader("fragment")]
float4 mainFS() : SV_Target
{
    return float4(float(kNumLights), 0.0, 0.0, 1.0);
}
)";

    ShaderVariantSchema MakeSpecSchema()
    {
        ShaderVariantSchema s;
        s.sources.push_back({Name("batch"), 16, 40});
        s.entries.push_back({Name("NUM_LIGHTS"), Name("batch"), 0, 4, 4});
        s.entries.back().symbol = Name("kNumLights");
        s.totalBits             = 56;
        return s;
    }

} // namespace

TEST(ShaderVariantTest, SpecConstantNotFoldedOnSpirv)
{
    auto                schema = MakeSpecSchema();
    ShaderCompilerSlang compiler;

    ShaderVariant v8;
    v8.entries.push_back({Name("NUM_LIGHTS"), 8});
    ShaderCompileDesc d8{};
    d8.source  = kSpecShader;
    d8.entry   = "mainFS";
    d8.stage   = ShaderStageFlagBit::FS;
    d8.target  = ShaderTarget::SPIRV;
    d8.variant = &v8;
    d8.schema  = &schema;
    ShaderCompileResult r8{};
    ASSERT_TRUE(compiler.Compile(d8, r8)) << r8.errorInfo;

    ShaderVariant v4;
    v4.entries.push_back({Name("NUM_LIGHTS"), 4});
    ShaderCompileDesc d4{};
    d4.source  = kSpecShader;
    d4.entry   = "mainFS";
    d4.stage   = ShaderStageFlagBit::FS;
    d4.target  = ShaderTarget::SPIRV;
    d4.variant = &v4;
    d4.schema  = &schema;
    ShaderCompileResult r4{};
    ASSERT_TRUE(compiler.Compile(d4, r4)) << r4.errorInfo;

    // spec value is NOT folded into SPIRV -> identical binary for value 8 vs 4
    EXPECT_EQ(r8.data, r4.data);
}

// DXIL requires the dxcompiler package (Win32-only in thirdparty.json)
#if defined(SKY_PLATFORM_WINDOWS)
TEST(ShaderVariantTest, SpecConstantFoldedOnDxil)
{
    auto                schema = MakeSpecSchema();
    ShaderCompilerSlang compiler;

    ShaderVariant v8;
    v8.entries.push_back({Name("NUM_LIGHTS"), 8});
    ShaderCompileDesc d8{};
    d8.source  = kSpecShader;
    d8.entry   = "mainFS";
    d8.stage   = ShaderStageFlagBit::FS;
    d8.target  = ShaderTarget::DXIL;
    d8.variant = &v8;
    d8.schema  = &schema;
    ShaderCompileResult r8{};
    ASSERT_TRUE(compiler.Compile(d8, r8)) << r8.errorInfo;

    ShaderVariant v4;
    v4.entries.push_back({Name("NUM_LIGHTS"), 4});
    ShaderCompileDesc d4{};
    d4.source  = kSpecShader;
    d4.entry   = "mainFS";
    d4.stage   = ShaderStageFlagBit::FS;
    d4.target  = ShaderTarget::DXIL;
    d4.variant = &v4;
    d4.schema  = &schema;
    ShaderCompileResult r4{};
    ASSERT_TRUE(compiler.Compile(d4, r4)) << r4.errorInfo;

    // spec value folds on DXIL (no native specialization) -> different binary
    EXPECT_NE(r8.data, r4.data);
}
#endif // SKY_PLATFORM_WINDOWS

TEST(ShaderVariantTest, BuildSpecialization)
{
    ShaderVariantSchema schema = MakeSpecSchema(); // NUM_LIGHTS = spec id 0
    schema.entries.push_back({Name("HAS_EMISSIVE"), Name("batch"), 4, 1, 0});

    ShaderVariant variant;
    variant.entries.push_back({Name("NUM_LIGHTS"), 8});
    variant.entries.push_back({Name("HAS_EMISSIVE"), 1});

    ShaderSpecialization spec;
    ShaderStageArtifact  artifact;
    artifact.identity.stage  = ShaderStageFlagBit::FS;
    artifact.variantBindings = {{"NUM_LIGHTS", "kNumLights", ShaderVariantType::UINT32, ShaderVariantLowering::NATIVE, 0, 4}};
    std::string error;
    ASSERT_TRUE(BuildShaderSpecialization(&variant, artifact, spec, error));

    ASSERT_EQ(spec.entries.size(), 1u);
    EXPECT_EQ(spec.entries[0].id, 0u);
    EXPECT_EQ(spec.entries[0].value, 8u);
}

TEST(ShaderVariantTest, ParseVariantBlock)
{
    const char *source = R"(
[shader("fragment")]
float4 mainFS() : SV_Target { return 1.0; }

// ===== @variant =====
// @source vertex
//   HAS_SKIN         : bool = 0
//   HAS_VERTEX_COLOR : bool = 0
// @source batch
//   HAS_EMISSIVE     : bool = 0
//   NUM_LIGHTS       : uint32(4) = 4
// ===================
)";

    ShaderVariantSchema schema;
    uint16_t            reservedBits = 0;
    std::string         error;
    ASSERT_TRUE(ShaderVariantGen::Parse(source, schema, reservedBits, error)) << error;

    EXPECT_EQ(reservedBits, 0u); // per-shader file: no @reserved

    ASSERT_EQ(schema.sources.size(), 2u);
    EXPECT_EQ(schema.sources[0].name, Name("vertex"));
    EXPECT_EQ(schema.sources[0].bitOffset, 0u); // relative offsets
    EXPECT_EQ(schema.sources[0].bitWidth, 2u);
    EXPECT_EQ(schema.sources[1].name, Name("batch"));
    EXPECT_EQ(schema.sources[1].bitOffset, 2u);
    EXPECT_EQ(schema.sources[1].bitWidth, 5u);

    ASSERT_EQ(schema.entries.size(), 4u);
    EXPECT_EQ(schema.entries[3].key, Name("NUM_LIGHTS"));
    EXPECT_EQ(schema.entries[3].bitWidth, 4u);
    EXPECT_EQ(schema.entries[3].defaultValue, 4u);
    EXPECT_EQ(schema.totalBits, 7u);
}

TEST(ShaderVariantTest, ParseReservedDirective)
{
    const char *source = R"(
// ===== @variant =====
// @reserved 16
// @source pipeline
//   SHADOWS : bool = 0
//   IBL     : bool = 0
// ===================
)";

    ShaderVariantSchema schema;
    uint16_t            reservedBits = 0;
    std::string         error;
    ASSERT_TRUE(ShaderVariantGen::Parse(source, schema, reservedBits, error)) << error;

    EXPECT_EQ(reservedBits, 16u);
    EXPECT_EQ(schema.totalBits, 2u); // two bool keys
    ASSERT_EQ(schema.entries.size(), 2u);
    EXPECT_EQ(schema.entries[0].key, Name("SHADOWS"));
    EXPECT_EQ(schema.entries[1].key, Name("IBL"));
}

TEST(ShaderVariantTest, GenerateVariantHeader)
{
    const char *source = R"(
// ===== @variant =====
// @source batch
//   HAS_EMISSIVE : bool = 0
//   NUM_LIGHTS   : uint32(4) = 4
// ===================
)";

    ShaderVariantSchema schema;
    uint16_t            reservedBits = 0;
    std::string         error;
    ASSERT_TRUE(ShaderVariantGen::Parse(source, schema, reservedBits, error)) << error;

    std::string header;
    ASSERT_TRUE(ShaderVariantGen::GenerateHeader("MyScene", schema, header));

    EXPECT_NE(header.find("GetMySceneVariantSchema()"), std::string::npos);
    EXPECT_NE(header.find("Name(\"HAS_EMISSIVE\")"), std::string::npos);
    EXPECT_NE(header.find("Name(\"NUM_LIGHTS\")"), std::string::npos);
}

TEST(ShaderVariantTest, VariantToString)
{
    ShaderVariant variant;
    variant.entries.push_back({Name("A"), 1});
    variant.entries.push_back({Name("B"), 2});

    EXPECT_EQ(variant.ToString(), "A=1, B=2");
}

TEST(ShaderVariantTest, KeyToString)
{
    auto             schema = MakeSchema();
    ShaderVariantKey key;
    key.Set(schema, Name("HAS_SKIN"), 1);
    key.Set(schema, Name("NUM_LIGHTS"), 5);

    const std::string s = key.ToString(schema);
    EXPECT_NE(s.find("HAS_SKIN=1"), std::string::npos);
    EXPECT_NE(s.find("NUM_LIGHTS=5"), std::string::npos);
    EXPECT_NE(s.find("HAS_VERTEX_COLOR=0"), std::string::npos);
}

TEST(ShaderVariantTest, VertexSemanticMask)
{
    VertexSemanticMask mask;
    mask.Set(VertexSemantic::POSITION, true);
    mask.Set(VertexSemantic::COLOR, true);

    EXPECT_TRUE(mask.Test(VertexSemantic::POSITION));
    EXPECT_TRUE(mask.Test(VertexSemantic::COLOR));
    EXPECT_FALSE(mask.Test(VertexSemantic::NORMAL));
    EXPECT_EQ(mask.ToString(), "POSITION,COLOR");
}

TEST(ShaderVariantTest, BuildVertexVariant)
{
    std::vector<VertexVariantDef> defs;
    defs.push_back({Name("HAS_VERTEX_COLOR"), {VertexSemantic::COLOR}});
    defs.push_back({Name("HAS_SKIN"), {VertexSemantic::JOINTS, VertexSemantic::WEIGHTS}});

    VertexSemanticMask mask;
    mask.Set(VertexSemantic::COLOR, true);
    mask.Set(VertexSemantic::JOINTS, true); // WEIGHTS missing

    ShaderVariant variant;
    BuildVertexVariant(defs, mask, variant);

    ASSERT_EQ(variant.entries.size(), 2u);
    EXPECT_EQ(variant.entries[0].key, Name("HAS_VERTEX_COLOR"));
    EXPECT_EQ(variant.entries[0].value, 1u);
    EXPECT_EQ(variant.entries[1].key, Name("HAS_SKIN"));
    EXPECT_EQ(variant.entries[1].value, 0u);
}

TEST(ShaderVariantTest, VertexSemanticsInKey)
{
    VertexSemanticMask mask;
    mask.Set(VertexSemantic::COLOR, true);
    mask.Set(VertexSemantic::UV1, true);

    ShaderVariantKey key;
    key.SetVertexSemantics(16, mask); // vertex region at offset 16

    const VertexSemanticMask read = key.GetVertexSemantics(16);
    EXPECT_TRUE(read.Test(VertexSemantic::COLOR));
    EXPECT_TRUE(read.Test(VertexSemantic::UV1));
    EXPECT_FALSE(read.Test(VertexSemantic::NORMAL));
}

TEST(ShaderVariantTest, ParseVertexBlock)
{
    const char *source = R"(
// ===== @vertex =====
//   HAS_VERTEX_COLOR : COLOR
//   HAS_SKIN         : JOINTS WEIGHTS
// ===================
)";

    std::vector<VertexVariantDef> defs;
    std::string                   error;
    ASSERT_TRUE(ShaderVariantGen::ParseVertex(source, defs, error)) << error;

    ASSERT_EQ(defs.size(), 2u);
    EXPECT_EQ(defs[0].name, Name("HAS_VERTEX_COLOR"));
    ASSERT_EQ(defs[0].semantics.size(), 1u);
    EXPECT_EQ(defs[0].semantics[0], VertexSemantic::COLOR);
    EXPECT_EQ(defs[1].name, Name("HAS_SKIN"));
    ASSERT_EQ(defs[1].semantics.size(), 2u);
    EXPECT_EQ(defs[1].semantics[0], VertexSemantic::JOINTS);
    EXPECT_EQ(defs[1].semantics[1], VertexSemantic::WEIGHTS);
}

TEST(ShaderVariantTest, TargetMappingsComeFromSourceDeclarations)
{
    auto                schema = MakeSpecSchema();
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   desc{};
    desc.source = kSpecShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.schema = &schema;
    for (const auto target : {ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL}) {
        SCOPED_TRACE(static_cast<int>(target));
        desc.target = target;
        ShaderCompileResult result;
        ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
        ASSERT_EQ(result.variantBindings.size(), 1u);
        const auto &binding = result.variantBindings[0];
        EXPECT_EQ(binding.key, "NUM_LIGHTS");
        EXPECT_EQ(binding.symbol, "kNumLights");
        EXPECT_EQ(binding.nativeId, 0u);
        EXPECT_EQ(binding.defaultValue, 4u);
        EXPECT_EQ(binding.type, ShaderVariantType::UINT32);
        EXPECT_EQ(binding.lowering, target == ShaderTarget::DXIL ? ShaderVariantLowering::COMPILE : ShaderVariantLowering::NATIVE);
    }
}

TEST(ShaderVariantTest, InvalidSourceAssociationsFailTransactionally)
{
    auto                schema = MakeSpecSchema();
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   desc{};
    desc.source = kSpecShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.schema = &schema;
    std::vector<ShaderVariantBinding> bindings{{"sentinel"}};
    std::string                       error;
    schema.entries[0].symbol = Name("missing");
    EXPECT_FALSE(compiler.InspectVariants(desc, bindings, error));
    EXPECT_NE(error.find("NUM_LIGHTS"), std::string::npos);
    EXPECT_EQ(bindings[0].key, "sentinel");
    schema.entries[0].symbol = Name("kNumLights");
    schema.entries[0].type   = ShaderVariantType::BOOL;
    EXPECT_FALSE(compiler.InspectVariants(desc, bindings, error));
    schema.entries[0].type = ShaderVariantType::UINT32;
    desc.source            = "[SpecializationConstant] [[vk::constant_id(0)]] const uint kNumLights = 7u; [shader(\"fragment\")] float4 "
                             "mainFS():SV_Target{return kNumLights;}";
    EXPECT_FALSE(compiler.InspectVariants(desc, bindings, error));
    EXPECT_NE(error.find("default"), std::string::npos);
}

TEST(ShaderVariantTest, MaterialDeclarationsCannotDisappearBehindCompileSwitches)
{
    ShaderVariantSchema schema;
    schema.sources   = {{Name("shader"), 0, 1}};
    schema.entries   = {{Name("ENABLED"), Name("shader"), 0, 1, 0}};
    schema.totalBits = 1;
    ShaderVariant     variant{{{Name("ENABLED"), 1}}};
    ShaderCompileDesc desc{};
    desc.schema  = &schema;
    desc.variant = &variant;
    desc.stage   = ShaderStageFlagBit::FS;
    desc.entry   = "Main";
    desc.source  = R"(
#ifndef ENABLED
#define ENABLED 0
#endif
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
#if ENABLED
struct Values { [AuroraField("tint")] float4 tint; };
[AuroraResource(2,0,"fixture/material")] ConstantBuffer<Values> values;
#endif
[shader("fragment")] float4 Main() : SV_Target {
#if ENABLED
return values.tint;
#else
return 1;
#endif
})";
    ShaderCompilerSlang compiler;
    ShaderCompileResult result;
    EXPECT_FALSE(compiler.Compile(desc, result));
    EXPECT_NE(result.errorInfo.find("stable"), std::string::npos) << result.errorInfo;
    EXPECT_TRUE(result.data.empty());
}

TEST(ShaderVariantTest, NativeThreadGroupShapeCannotBePublishedAsFixedMetadata)
{
    auto              schema = MakeSpecSchema();
    ShaderCompileDesc desc{};
    desc.schema = &schema;
    desc.stage  = ShaderStageFlagBit::CS;
    desc.entry  = "Main";
    desc.source = R"(
#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif
[SpecializationConstant] [[vk::constant_id(0)]] const uint kNumLights = NUM_LIGHTS;
[shader("compute")] [numthreads(kNumLights,1,1)] void Main(uint3 id : SV_DispatchThreadID) {}
)";
    ShaderCompilerSlang compiler;
    ShaderCompileResult result;
    EXPECT_FALSE(compiler.Compile(desc, result));
    EXPECT_TRUE(result.data.empty());
    EXPECT_FALSE(result.errorInfo.empty());
}
