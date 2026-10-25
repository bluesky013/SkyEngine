//
// Slang spike: compile a mini shader with ParameterBlocks to SPIRV + MSL.
//

#include <aurora/shader/ShaderCompilerSlang.h>

#include <core/platform/Platform.h>

#include <gtest/gtest.h>
#include <slang-com-ptr.h>
#include <slang.h>

#include <cstring>
#include <map>
#include <string>

using namespace sky;
using namespace sky::aurora;

namespace {

    const char *kIdentity = R"(
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
)";

    const std::string kSpikeShader = std::string(kIdentity) + R"(
struct GlobalParams { [AuroraField("viewProj")] float4x4 viewProj; [AuroraField("cameraPos")] float4 cameraPos; };
struct PassParams { [AuroraField("params")] float4 params; };

[AuroraResource(1,0,"fixture/global")] [[vk::binding(0,1)]] ConstantBuffer<GlobalParams> gGlobal;
[AuroraResource(2,0,"fixture/pass")] [[vk::binding(0,2)]] ConstantBuffer<PassParams> gPass;
[AuroraResource(0,0,"fixture/texture")] [[vk::binding(0,0)]] Texture2D<float4> gTex;
[AuroraResource(0,1,"fixture/sampler")] [[vk::binding(1,0)]] SamplerState gSmp;

struct FSOutput { float4 color : SV_Target; };

[shader("fragment")]
FSOutput mainFS(float2 uv : TEXCOORD0)
{
    FSOutput o;
    o.color = gTex.Sample(gSmp, uv) * gPass.params + gGlobal.cameraPos;
    return o;
}
)";

    std::string WordsToText(const std::vector<uint8_t> &data)
    {
        std::string out(data.size(), '\0');
        std::memcpy(out.data(), data.data(), out.size());
        out.erase(out.find_last_not_of('\0') + 1);
        return out;
    }

} // namespace

TEST(SlangSpikeTest, MetalKeepsLogicalBindingsAndReflectsActualArgumentSlots)
{
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   desc{};
    desc.source = std::string(kIdentity) + R"(
        struct Params { [AuroraField("value")] float4 value; };
        [AuroraResource(1,0,"fixture/slots")] [[vk::binding(0, 1)]] ConstantBuffer<Params> first;
        [AuroraResource(2,0,"fixture/slots")] [[vk::binding(0, 2)]] ConstantBuffer<Params> second;
        [AuroraResource(1,3,"fixture/slots")] [[vk::binding(3, 1)]] ConstantBuffer<Params> unused;
        [AuroraResource(1,4,"fixture/slots")] [[vk::binding(4, 1)]] Texture2D<float4> sampled;
        [AuroraResource(1,5,"fixture/slots")] [[vk::binding(5, 1)]] RWTexture2D<float4> outputImage;
        [AuroraResource(1,7,"fixture/slots")] [[vk::binding(7, 1)]] SamplerState sampler : register(s7, space1);
        [numthreads(1, 1, 1)] void Fill(uint3 id : SV_DispatchThreadID) {
            outputImage[id.xy] = sampled.SampleLevel(sampler, float2(0.5), 0) + first.value + second.value;
        }
    )";
    desc.entry  = "Fill";
    desc.stage  = ShaderStageFlagBit::CS;
    desc.target = ShaderTarget::MSL;
    ShaderCompileResult result;
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    ASSERT_EQ(result.reflection.resources.size(), 5u);
    ASSERT_EQ(result.reflection.nativeBindings.size(), 5u);
    auto slot = [&](uint32_t set, uint32_t binding) {
        for (const auto &native : result.reflection.nativeBindings)
            if (native.set == set && native.binding == binding) {
                EXPECT_EQ(native.stage, ShaderStageFlagBit::CS);
                EXPECT_EQ(native.space, 0u);
                return native.index;
            }
        ADD_FAILURE() << "Missing native slot " << set << ":" << binding;
        return UINT32_MAX;
    };
    const auto source = WordsToText(result.data);
    EXPECT_NE(slot(1, 0), slot(2, 0));
    EXPECT_NE(source.find("[[buffer(" + std::to_string(slot(1, 0)) + ")]]"), std::string::npos);
    EXPECT_NE(source.find("[[buffer(" + std::to_string(slot(2, 0)) + ")]]"), std::string::npos);
    EXPECT_NE(source.find("[[texture(" + std::to_string(slot(1, 5)) + ")]]"), std::string::npos);
    EXPECT_EQ(slot(1, 7), 7u);
    EXPECT_NE(source.find("[[sampler(7)]]"), std::string::npos);
}

TEST(SlangSpikeTest, MetalRejectsNativeRegisterSpaceCollisionsAndAcceptsTargetAllocatedSlots)
{
    const std::string   body = R"(
struct Params { [AuroraField("value")] float4 value; };
[AuroraResource(0,0,"fixture/registers")] [[vk::binding(0,0)]] ConstantBuffer<Params> first FIRST_REGISTER;
[AuroraResource(2,0,"fixture/registers")] [[vk::binding(0,2)]] ConstantBuffer<Params> second SECOND_REGISTER;
float4 VSMain(float2 position : POSITION) : SV_Position { return float4(position, 0, 1) + first.value + second.value; }
)";
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   desc{};
    desc.source = std::string(kIdentity) + "#define FIRST_REGISTER : register(b0, space0)\n#define SECOND_REGISTER : register(b0, space2)\n" + body;
    desc.entry  = "VSMain";
    desc.stage  = ShaderStageFlagBit::VS;
    desc.target = ShaderTarget::SPIRV;
    ShaderCompileResult result;
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    ASSERT_EQ(result.reflection.resources.size(), 2u);
    desc.target = ShaderTarget::MSL;
    EXPECT_FALSE(compiler.Compile(desc, result));
    EXPECT_NE(result.errorInfo.find("Unsupported Metal direct bindings"), std::string::npos) << result.errorInfo;
    EXPECT_TRUE(result.data.empty());
    EXPECT_EQ(result.artifactId, 0u);

    desc.source = std::string(kIdentity) + R"(
#if AURORA_TARGET_DXIL
#define FIRST_REGISTER : register(b0, space0)
#define SECOND_REGISTER : register(b0, space2)
#else
#define FIRST_REGISTER
#define SECOND_REGISTER
#endif
)" + body;
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    ASSERT_EQ(result.reflection.resources.size(), 2u);
    ASSERT_EQ(result.reflection.nativeBindings.size(), 2u);
    const auto &first  = result.reflection.nativeBindings[0];
    const auto &second = result.reflection.nativeBindings[1];
    EXPECT_EQ(first.set, 0u);
    EXPECT_EQ(second.set, 2u);
    EXPECT_EQ(first.space, 0u);
    EXPECT_EQ(second.space, 0u);
    EXPECT_NE(first.index, second.index);
    const auto source = WordsToText(result.data);
    EXPECT_NE(source.find("[[buffer(" + std::to_string(first.index) + ")]]"), std::string::npos);
    EXPECT_NE(source.find("[[buffer(" + std::to_string(second.index) + ")]]"), std::string::npos);
}

TEST(SlangSpikeTest, ResourceShapesArraysAndSamplerKinds)
{
    const std::string   source = std::string(kIdentity) + R"(
        [AuroraResource(0,0,"fixture/shapes")] [[vk::binding(0,0)]] Texture2D<float4> color;
        [AuroraResource(0,1,"fixture/shapes")] [[vk::binding(1,0)]] Texture2DArray<float4> layers;
        [AuroraResource(0,2,"fixture/shapes")] [[vk::binding(2,0)]] Texture2D<uint4> integers;
        [AuroraResource(0,3,"fixture/shapes")] [[vk::binding(3,0)]] Texture2D<float3> rgb;
        [AuroraResource(0,4,"fixture/shapes")] [[vk::binding(4,0)]] Texture2DMS<float4> multisample;
        [AuroraResource(0,5,"fixture/shapes")] [[vk::binding(5,0)]] Texture3D<float4> volume;
        [AuroraResource(0,6,"fixture/shapes")] [[vk::binding(6,0)]] TextureCube<float4> cube;
        [AuroraResource(0,7,"fixture/shapes")] [[vk::binding(7,0)]] Texture2D<float4> descriptors[2];
        [AuroraResource(0,8,"fixture/shapes")] [[vk::binding(8,0)]] Texture2D<float4> oneDescriptor[1];
        [AuroraResource(0,9,"fixture/shapes")] [[vk::binding(9,0)]] Texture2D<float4> unbounded[];
        [AuroraResource(0,10,"fixture/shapes")] [[vk::binding(10,0)]] RWTexture2D<float4> storage;
        [AuroraResource(0,11,"fixture/shapes")] [[vk::binding(11,0)]] StructuredBuffer<float4> buffer;
        [AuroraResource(0,12,"fixture/shapes")] [[vk::binding(12,0)]] SamplerState regular;
        [AuroraResource(0,13,"fixture/shapes")] [[vk::binding(13,0)]] SamplerComparisonState comparison;
    )";
    ShaderCompilerSlang compiler;
    for (auto target : {ShaderTarget::SPIRV, ShaderTarget::DXIL, ShaderTarget::MSL}) {
        ShaderReflection reflection;
        std::string      error;
        ASSERT_TRUE(compiler.ReflectBlocks(source, target, reflection, error)) << error;
        ASSERT_EQ(reflection.declarations.size(), 14u);
        auto resource = [&](const char *name) -> ShaderResource {
            for (const auto &value : reflection.declarations)
                if (value.name == name)
                    return value;
            ADD_FAILURE() << name;
            return {};
        };
        EXPECT_EQ(resource("color").texture.dimension, ShaderTextureDimension::D2);
        EXPECT_EQ(resource("color").texture.scalarType, ShaderScalarType::FLOAT);
        EXPECT_EQ(resource("color").texture.components, 4u);
        EXPECT_FALSE(resource("color").texture.arrayed);
        EXPECT_TRUE(resource("layers").texture.arrayed);
        EXPECT_EQ(resource("integers").texture.scalarType, ShaderScalarType::UINT);
        EXPECT_EQ(resource("rgb").texture.components, 3u);
        EXPECT_TRUE(resource("multisample").texture.multisampled);
        EXPECT_EQ(resource("volume").texture.dimension, ShaderTextureDimension::D3);
        EXPECT_EQ(resource("cube").texture.dimension, ShaderTextureDimension::CUBE);
        EXPECT_EQ(resource("descriptors").count, 2u);
        EXPECT_TRUE(resource("descriptors").descriptorArray);
        EXPECT_EQ(resource("oneDescriptor").count, 1u);
        EXPECT_TRUE(resource("oneDescriptor").descriptorArray);
        EXPECT_EQ(resource("unbounded").count, 0u);
        EXPECT_TRUE(resource("unbounded").descriptorArray);
        EXPECT_EQ(resource("storage").type, ShaderResourceType::STORAGE_IMAGE);
        EXPECT_EQ(resource("buffer").type, ShaderResourceType::STORAGE_BUFFER);
        EXPECT_EQ(resource("regular").sampler, ShaderSamplerType::REGULAR);
        EXPECT_EQ(resource("comparison").sampler, ShaderSamplerType::COMPARISON);
    }
}

TEST(SlangSpikeTest, ResourceContainersFailWithoutPublishingPartialReflection)
{
    ShaderCompilerSlang compiler;
    for (auto target : {ShaderTarget::SPIRV, ShaderTarget::DXIL, ShaderTarget::MSL}) {
        ShaderReflection reflection;
        reflection.complete = true;
        std::string error;
        EXPECT_FALSE(compiler.ReflectBlocks(std::string(kIdentity) + R"(
            struct Nested { [AuroraField("value")] float4 value; Texture2D<float4> image; };
            [AuroraResource(1,0,"fixture/nested")] ParameterBlock<Nested> nested;
        )",
                                            target, reflection, error));
        EXPECT_FALSE(error.empty());
        EXPECT_FALSE(reflection.complete);
        EXPECT_TRUE(reflection.declarations.empty());
    }
}

TEST(SlangSpikeTest, CompileSpirvWithParameterBlocks)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::SPIRV;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    // slang natural layout: ordinary resources -> set 0; each ParameterBlock gets
    // its own descriptor set in declaration order (global=1, pass=2)
    bool foundGlobal = false, foundPass = false, foundTex = false;
    for (const auto &res : result.reflection.resources) {
        if (res.set == 1 && res.binding == 0 && res.type == ShaderResourceType::UNIFORM_BUFFER) {
            foundGlobal = true;
        }
        if (res.set == 2 && res.binding == 0 && res.type == ShaderResourceType::UNIFORM_BUFFER) {
            foundPass = true;
        }
        if (res.set == 0 && res.binding == 0 && res.type == ShaderResourceType::SAMPLED_IMAGE) {
            foundTex = true;
        }
    }
    EXPECT_TRUE(foundGlobal);
    EXPECT_TRUE(foundPass);
    EXPECT_TRUE(foundTex);

    // UBO member reflection: blocks carry field name/offset/size
    const ShaderBlockLayout *globalBlock = nullptr;
    for (const auto &block : result.reflection.blocks) {
        if (block.name == "gGlobal") {
            globalBlock = &block;
        }
    }
    ASSERT_NE(globalBlock, nullptr);
    ASSERT_EQ(globalBlock->members.size(), 2u);
    EXPECT_STREQ(globalBlock->members[0].name.c_str(), "viewProj");
    EXPECT_EQ(globalBlock->members[0].offset, 0u);
    EXPECT_EQ(globalBlock->members[0].size, 64u);
    EXPECT_STREQ(globalBlock->members[1].name.c_str(), "cameraPos");
    EXPECT_EQ(globalBlock->members[1].offset, 64u); // after mat4
    EXPECT_EQ(globalBlock->members[1].size, 16u);
}

// DX12 backend: DXIL via slang's runtime DXC loader (repo's dxcompiler package)
// DXIL requires the dxcompiler package (Win32-only in thirdparty.json)
#if defined(SKY_PLATFORM_WINDOWS)
TEST(SlangSpikeTest, CompileDxilForDx12)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::DXIL;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    // DXIL binary starts with the DXBC magic "DXBC"
    const auto &words = result.data;
    ASSERT_GE(words.size(), 1u);
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(words.data());
    EXPECT_EQ(bytes[0], 'D');
    EXPECT_EQ(bytes[1], 'X');
    EXPECT_EQ(bytes[2], 'B');
    EXPECT_EQ(bytes[3], 'C');

    // slang reflection on DXIL path: ParameterBlocks still visible
    EXPECT_FALSE(result.reflection.resources.empty());
}
#endif // SKY_PLATFORM_WINDOWS

TEST(SlangSpikeTest, CompileMslDirectNoSpirvCross)
{
    ShaderCompilerSlang compiler;

    ShaderCompileDesc desc{};
    desc.source = kSpikeShader;
    desc.entry  = "mainFS";
    desc.stage  = ShaderStageFlagBit::FS;
    desc.target = ShaderTarget::MSL;

    ShaderCompileResult result{};
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    EXPECT_FALSE(result.data.empty());

    const std::string msl = WordsToText(result.data);
    // ParameterBlock -> argument buffer style: constant pointer at [[buffer(N)]]
    EXPECT_NE(msl.find("[[buffer("), std::string::npos);
    EXPECT_NE(msl.find("constant"), std::string::npos);
    EXPECT_NE(msl.find("[[fragment]]"), std::string::npos);
    EXPECT_NE(msl.find("metal_stdlib"), std::string::npos);
    // #line directives are disabled (SLANG_LINE_DIRECTIVE_MODE_NONE)
    EXPECT_EQ(msl.find("#line"), std::string::npos);
}

namespace {
    const char *kIdentityLayoutShader = R"AURORA_GATE(
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraResourceAttribute { int logicalSet; int logicalBinding; string owner; };
[__AttributeUsage(_AttributeTargets.Var)]
struct AuroraFieldAttribute { string path; };
struct Params {
    [AuroraField( "weight")] float weight;
    [AuroraField( "uv")] float2 uv;
    [AuroraField( "rowTransform")] row_major float4x4 rowTransform;
    [AuroraField( "columnTransform")] column_major float4x4 columnTransform;
    [AuroraField( "taps")] float4 taps[3];
};
namespace First {
    [AuroraResource(1, 0, "first")]
    [[vk::binding(0, 1)]] ConstantBuffer<Params> gShared;
}
namespace Second {
    [AuroraResource(2, 0, "second")]
    [[vk::binding(0, 2)]] ConstantBuffer<Params> gShared;
}
[AuroraResource(0, 0, "global")]
[[vk::binding(0, 0)]] Texture2D<float4> gTexture;
[AuroraResource(0, 1, "global")]
[[vk::binding(1, 0)]] SamplerState gSampler;
[AuroraResource(1, 2, "compute")]
[[vk::binding(2, 1)]] RWTexture2D<float4> gOutput;
[AuroraResource(1, 3, "compute")]
[[vk::binding(3, 1)]] StructuredBuffer<float4> gRead;
[AuroraResource(1, 4, "compute")]
[[vk::binding(4, 1)]] RWStructuredBuffer<float4> gWrite;
float4 ReadFirst() { return mul(First::gShared.rowTransform, float4(1,2,3,1)) + mul(First::gShared.columnTransform, float4(3,2,1,1)) + First::gShared.taps[2] + float4(First::gShared.uv, First::gShared.weight, 1); }
float4 ReadSecond() { return mul(Second::gShared.rowTransform, float4(1,2,3,1)) + mul(Second::gShared.columnTransform, float4(3,2,1,1)) + Second::gShared.taps[2] + float4(Second::gShared.uv, Second::gShared.weight, 1); }
float4 VSMain(uint id : SV_VertexID) : SV_Position { return ReadFirst() + gRead[id]; }
float4 FSMain(float4 position : SV_Position) : SV_Target {
#if USE_FIRST
    return ReadFirst() + gTexture.SampleLevel(gSampler, float2(.5,.5), 0);
#else
    return ReadSecond() + gTexture.SampleLevel(gSampler, float2(.5,.5), 0);
#endif
}
[numthreads(1, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID) { float4 value = ReadFirst() + ReadSecond() + gRead[id.x]; gOutput[id.xy] = value; gWrite[id.x] = value; }
)AURORA_GATE";
}

TEST(SlangSpikeTest, DeclarationIdentityAndNativeBlockAbiComeFromEachRequestedTarget)
{
    ShaderCompilerSlang compiler;
    for (const auto target : {ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL}) {
        SCOPED_TRACE(static_cast<uint32_t>(target));
        ShaderReflection reflection;
        std::string      error;
        ASSERT_TRUE(compiler.ReflectBlocks(kIdentityLayoutShader, target, reflection, error)) << error;
        EXPECT_TRUE(reflection.complete);
        EXPECT_EQ(reflection.kind, ShaderReflectionKind::DECLARATION);
        EXPECT_FALSE(reflection.stages);
        EXPECT_TRUE(reflection.resources.empty());
        EXPECT_TRUE(reflection.nativeBindings.empty());
        ASSERT_EQ(reflection.declarations.size(), 7u);
        ASSERT_EQ(reflection.declaredBlocks.size(), 2u);
        for (const auto &block : reflection.declaredBlocks) {
            ASSERT_EQ(block.members.size(), 5u);
            EXPECT_EQ(block.size, 192u);
            EXPECT_EQ(block.members[1].offset, target == ShaderTarget::DXIL ? 4u : 8u);
            EXPECT_EQ(block.members[2].matrixStride, 16u);
            EXPECT_EQ(block.members[2].matrixLayout, ShaderMatrixLayout::ROW_MAJOR);
            EXPECT_EQ(block.members[3].matrixStride, 16u);
            EXPECT_EQ(block.members[3].matrixLayout, ShaderMatrixLayout::COLUMN_MAJOR);
            EXPECT_EQ(block.members[4].arrayCount, 3u);
            EXPECT_EQ(block.members[4].arrayStride, 16u);
        }
        size_t sameShortName = 0;
        for (const auto &declaration : reflection.declarations)
            if (declaration.name == "gShared") {
                ++sameShortName;
                EXPECT_EQ(declaration.binding, 0u);
                EXPECT_EQ(declaration.declarationOwner, declaration.set == 1 ? "first" : "second");
            }
        EXPECT_EQ(sameShortName, 2u);
    }
}

TEST(SlangSpikeTest, CompleteStageArtifactsPreserveIdentityThroughStageMacroAndOptimization)
{
    ShaderCompilerSlang       compiler;
    std::vector<ShaderTarget> targets{ShaderTarget::SPIRV, ShaderTarget::MSL};
#if defined(SKY_PLATFORM_WINDOWS)
    targets.push_back(ShaderTarget::DXIL);
#endif
    for (const auto target : targets)
        for (const auto stage : {ShaderStageFlagBit::VS, ShaderStageFlagBit::FS, ShaderStageFlagBit::CS})
            for (const uint32_t first : {0u, 1u})
                for (const uint32_t optimization : {0u, 3u}) {
                    SCOPED_TRACE(std::to_string(static_cast<uint32_t>(target)) + ":" + std::to_string(static_cast<uint32_t>(stage)) + ":" +
                                 std::to_string(first) + ":" + std::to_string(optimization));
                    ShaderVariant variant;
                    variant.entries.push_back({Name("USE_FIRST"), first});
                    ShaderCompileDesc desc{};
                    desc.source      = kIdentityLayoutShader;
                    desc.sourcePath  = "fixture/IdentityLayout.slang";
                    desc.entry       = stage == ShaderStageFlagBit::VS ? "VSMain" : stage == ShaderStageFlagBit::FS ? "FSMain" : "CSMain";
                    desc.stage       = stage;
                    desc.target      = target;
                    desc.variant     = &variant;
                    desc.environment = DefaultShaderEnvironment(target);
                    desc.environment.options.push_back("optimization=" + std::to_string(optimization));
                    ShaderCompileResult result;
                    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
                    std::string error;
                    ASSERT_TRUE(ValidateShaderArtifact(result, error)) << error;
                    EXPECT_EQ(result.identity.sourceEntry, desc.entry);
                    EXPECT_EQ(result.identity.nativeEntry, target == ShaderTarget::SPIRV ? "main" : desc.entry);
                    EXPECT_EQ(result.identity.producer, compiler.GetProducerIdentity(target));
                    EXPECT_NE(result.identity.producer.configuration, 0u);
                    EXPECT_FALSE(result.identity.producer.version.empty());
                    EXPECT_EQ(result.reflection.kind, ShaderReflectionKind::STAGE);
                    EXPECT_EQ(result.reflection.stages, ShaderStageFlags(stage));
                    ASSERT_EQ(result.reflection.declarations.size(), 7u);
                    ASSERT_EQ(result.reflection.declaredBlocks.size(), 2u);
                    EXPECT_EQ(result.reflection.nativeBindings.size(), result.reflection.resources.size());
                    auto active = [&](uint32_t set, uint32_t binding) {
                        for (const auto &resource : result.reflection.resources)
                            if (resource.set == set && resource.binding == binding)
                                return true;
                        return false;
                    };
                    EXPECT_EQ(active(1, 0), stage != ShaderStageFlagBit::FS || first != 0);
                    EXPECT_EQ(active(2, 0), stage == ShaderStageFlagBit::CS || (stage == ShaderStageFlagBit::FS && first == 0));
                    EXPECT_EQ(active(1, 2), stage == ShaderStageFlagBit::CS);
                    for (const auto &native : result.reflection.nativeBindings) {
                        EXPECT_EQ(native.stage, stage);
                        EXPECT_NE(native.category, ShaderNativeCategory::UNKNOWN);
                        EXPECT_EQ(native.count, 1u);
                        // DXIL and Metal use their own native address spaces; Vulkan keeps the logical set.
                        EXPECT_EQ(native.space, target == ShaderTarget::DXIL || target == ShaderTarget::MSL ? 0u : native.set);
                    }
                    auto corrupted = result;
                    corrupted.data.back() ^= 1;
                    EXPECT_FALSE(ValidateShaderArtifact(corrupted, error));
                }
}

TEST(SlangSpikeTest, StableFieldIdentityDoesNotDependOnAuthoredPhysicalMemberName)
{
    ShaderCompilerSlang compiler;
    ShaderReflection    reflection;
    std::string         error;
    ASSERT_TRUE(compiler.ReflectBlocks(std::string(kIdentity) + R"(
        struct Params { [AuroraField("canonical")] float4 differentPhysicalName; };
        [AuroraResource(1,0,"fixture/identity")] ConstantBuffer<Params> params;
    )",
                                       ShaderTarget::MSL, reflection, error))
        << error;
    ASSERT_EQ(reflection.declaredBlocks.size(), 1u);
    ASSERT_EQ(reflection.declaredBlocks.front().members.size(), 1u);
    EXPECT_EQ(reflection.declaredBlocks.front().members.front().name, "canonical");
}

TEST(SlangSpikeTest, MissingOrDuplicateIdentityAndUnsupportedMetalArrayPublishNoArtifact)
{
    const char *cases[] = {
        R"(RWTexture2D<float4> image; [numthreads(1,1,1)]void Fill(uint3 id:SV_DispatchThreadID){image[id.xy]=float4(1);})",
        R"(struct Params{float4 value;};[AuroraResource(1,0,"fixture")]ConstantBuffer<Params> params;
           [AuroraResource(1,1,"fixture")]RWTexture2D<float4> image;
           [numthreads(1,1,1)]void Fill(uint3 id:SV_DispatchThreadID){image[id.xy]=params.value;})",
        R"([AuroraResource(1,0,"fixture/a")]RWTexture2D<float4> a;[AuroraResource(1,0,"fixture/b")]RWTexture2D<float4> b;
           [numthreads(1,1,1)]void Fill(uint3 id:SV_DispatchThreadID){a[id.xy]=float4(1);b[id.xy]=float4(2);})",
        R"([AuroraResource(1,0,"fixture")]RWTexture2D<float4> image[1];
           [numthreads(1,1,1)]void Fill(uint3 id:SV_DispatchThreadID){image[0][id.xy]=float4(1);})",
    };
    ShaderCompilerSlang compiler;
    for (const auto *source : cases) {
        SCOPED_TRACE(source);
        ShaderCompileDesc desc{};
        desc.source = std::string(kIdentity) + source;
        desc.entry  = "Fill";
        desc.stage  = ShaderStageFlagBit::CS;
        desc.target = ShaderTarget::MSL;
        ShaderCompileResult result;
        result.data.push_back(1);
        result.artifactId          = 42;
        result.reflection.complete = true;
        EXPECT_FALSE(compiler.Compile(desc, result));
        EXPECT_FALSE(result.errorInfo.empty());
        EXPECT_TRUE(result.data.empty());
        EXPECT_EQ(result.artifactId, 0u);
        EXPECT_FALSE(result.reflection.complete);
    }
}

TEST(SlangSpikeTest, ReadOnlyAndReadWriteBufferSemanticsKeepNativeClasses)
{
    ShaderCompilerSlang compiler;
    const std::string   source = std::string(kIdentity) + R"(
        [AuroraResource(1,0,"fixture/buffers")] [[vk::binding(0,1)]] StructuredBuffer<float4> inputData;
        [AuroraResource(1,1,"fixture/buffers")] [[vk::binding(1,1)]] RWStructuredBuffer<float4> outputData;
        [AuroraResource(1,2,"fixture/buffers")] [[vk::binding(2,1)]] ByteAddressBuffer rawInput;
        [AuroraResource(1,3,"fixture/buffers")] [[vk::binding(3,1)]] RWByteAddressBuffer rawOutput;
    )";
    for (const auto target : {ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL}) {
        ShaderReflection reflection;
        std::string      error;
        ASSERT_TRUE(compiler.ReflectBlocks(source, target, reflection, error)) << error;
        ASSERT_EQ(reflection.declarations.size(), 4u);
        for (const auto &resource : reflection.declarations) {
            EXPECT_EQ(resource.type, ShaderResourceType::STORAGE_BUFFER);
            EXPECT_EQ(resource.access, resource.binding % 2 ? ShaderResourceAccess::READ_WRITE : ShaderResourceAccess::READ_ONLY);
            EXPECT_EQ(resource.bufferKind, resource.binding < 2 ? ShaderBufferKind::STRUCTURED : ShaderBufferKind::RAW);
            EXPECT_EQ(resource.elementStride, resource.binding < 2 ? 16u : 0u);
        }
    }
}

TEST(SlangSpikeTest, UnsupportedEnvironmentIsRejectedBeforePublishingCode)
{
    ShaderCompilerSlang compiler;
    ShaderCompileDesc   desc{};
    desc.source              = "[numthreads(1,1,1)] void Empty() {}";
    desc.entry               = "Empty";
    desc.stage               = ShaderStageFlagBit::CS;
    desc.target              = ShaderTarget::MSL;
    desc.environment         = DefaultShaderEnvironment(desc.target);
    desc.environment.options = {"unimplemented-native-option"};
    ShaderCompileResult result;
    EXPECT_FALSE(compiler.Compile(desc, result));
    EXPECT_TRUE(result.data.empty());
    EXPECT_NE(result.errorInfo.find("Unsupported"), std::string::npos);
}

int main(int argc, char *argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


TEST(SlangSpikeTest, VariantDeclarationReflectionGate)
{
    Slang::ComPtr<slang::IGlobalSession> global;
    ASSERT_TRUE(SLANG_SUCCEEDED(slang::createGlobalSession(global.writeRef())));
    EXPECT_STREQ(global->getBuildTagString(), "2026.19");
    const char *source = R"(
#ifndef NUM_LIGHTS
#define NUM_LIGHTS 4
#endif
#if AURORA_TARGET_DXIL
static const int kNumLights = NUM_LIGHTS;
static const bool kEnabled = true;
static const uint kUnused = 9u;
#else
[SpecializationConstant] [[vk::constant_id(7)]] const int kNumLights = NUM_LIGHTS;
[SpecializationConstant] [[vk::constant_id(2)]] const bool kEnabled = true;
[SpecializationConstant] [[vk::constant_id(31)]] const uint kUnused = 9u;
#endif
[shader("fragment")] float4 Main() : SV_Target { return kEnabled ? float4(kNumLights, 0, 0, 1) : 0; }
)";
    for (auto target : {SLANG_SPIRV, SLANG_METAL, SLANG_DXIL}) {
        SCOPED_TRACE(static_cast<int>(target));
        slang::TargetDesc targetDesc{};
        targetDesc.format = target;
        targetDesc.profile = global->findProfile(target == SLANG_METAL ? "metallib_2_3" : "sm_6_0");
        const slang::PreprocessorMacroDesc macros[] = {
            {"AURORA_TARGET_DXIL", target == SLANG_DXIL ? "1" : "0"},
            {"NUM_LIGHTS", target == SLANG_DXIL ? "8" : "4"},
        };
        slang::SessionDesc sessionDesc{};
        sessionDesc.targets = &targetDesc;
        sessionDesc.targetCount = 1;
        sessionDesc.preprocessorMacros = macros;
        sessionDesc.preprocessorMacroCount = 2;
        Slang::ComPtr<slang::ISession> session;
        ASSERT_TRUE(SLANG_SUCCEEDED(global->createSession(sessionDesc, session.writeRef())));
        Slang::ComPtr<slang::IBlob> diagnostics;
        auto *module = session->loadModuleFromSourceString("variant_gate", "variant_gate.slang", source, diagnostics.writeRef());
        ASSERT_NE(module, nullptr) << (diagnostics ? static_cast<const char *>(diagnostics->getBufferPointer()) : "");
        std::map<std::string, slang::VariableReflection *> declarations;
        for (auto *child : module->getModuleReflection()->getChildren())
            if (auto *variable = child->asVariable())
                declarations.emplace(variable->getName(), variable);
        const struct { const char *name; slang::TypeReflection::ScalarType type; uint32_t bits; uint32_t id; } expected[] = {
            {"kNumLights", slang::TypeReflection::ScalarType::Int32, target == SLANG_DXIL ? 8u : 4u, 7},
            {"kEnabled", slang::TypeReflection::ScalarType::Bool, 1, 2},
            {"kUnused", slang::TypeReflection::ScalarType::UInt32, 9, 31},
        };
        auto *layout = module->getLayout();
        ASSERT_NE(layout, nullptr);
        std::map<std::string, slang::VariableLayoutReflection *> parameters;
        for (unsigned i = 0; i < layout->getParameterCount(); ++i) {
            auto *parameter = layout->getParameterByIndex(i);
            parameters.emplace(parameter->getName(), parameter);
        }
        for (const auto &item : expected) {
            SCOPED_TRACE(item.name);
            ASSERT_NE(declarations.find(item.name), declarations.end());
            auto *variable = declarations.at(item.name);
            EXPECT_EQ(variable->getType()->getKind(), slang::TypeReflection::Kind::Scalar);
            EXPECT_EQ(variable->getType()->getScalarType(), item.type);
            Slang::ComPtr<slang::IBlob> value;
            ASSERT_TRUE(SLANG_SUCCEEDED(variable->getDefaultValueBlob(value.writeRef())));
            ASSERT_NE(value.get(), nullptr);
            ASSERT_EQ(value->getBufferSize(), sizeof(uint32_t));
            uint32_t bits = 0;
            std::memcpy(&bits, value->getBufferPointer(), sizeof(bits));
            EXPECT_EQ(bits, item.bits);
            if (target == SLANG_DXIL) {
                EXPECT_EQ(parameters.find(item.name), parameters.end());
            } else {
                ASSERT_NE(parameters.find(item.name), parameters.end());
                auto *parameter = parameters.at(item.name);
                EXPECT_EQ(parameter->getCategory(), slang::ParameterCategory::SpecializationConstant);
                EXPECT_EQ(parameter->getOffset(slang::ParameterCategory::SpecializationConstant), item.id);
            }
        }
    }
}
