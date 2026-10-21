#include "GlobalBlock.h"
#include "OpaquePass.h"
#include "SceneBatch.h"
#include "SceneEnvironmentPass.h"
#include "SceneLightingPass.h"
#include "ScenePass.h"
#include "SceneSurfaceBatch.h"
#include "ShadowPass.h"
#include "SharedShaderFixture.h"
#include "TextureToScreenPass.h"
#include "ToneMapPass.h"
#include <algorithm>
#include <aurora/pipeline/SceneMaterial.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <filesystem>
#include <gtest/gtest.h>

using namespace sky;
using namespace sky::aurora;

namespace {
    struct Declaration {
        const char             *header;
        const ShaderReflection &schema;
        const char             *expression;
    };

    void CheckResource(const ShaderResource &expected, const ShaderResource &actual)
    {
        EXPECT_EQ(actual.name, expected.name);
        EXPECT_EQ(actual.type, expected.type);
        EXPECT_EQ(actual.count, expected.count);
        EXPECT_EQ(actual.descriptorArray, expected.descriptorArray);
        EXPECT_EQ(actual.texture, expected.texture);
        EXPECT_EQ(actual.sampler, expected.sampler);
    }

    void CheckBlock(const ShaderBlockLayout &expected, const ShaderBlockLayout &actual)
    {
        EXPECT_EQ(actual.name, expected.name);
        EXPECT_EQ(actual.size, expected.size);
        EXPECT_EQ(actual.members, expected.members);
    }

    void CheckSchema(const ShaderReflection &expected, const ShaderReflection &actual)
    {
        ASSERT_EQ(actual.blocks.size(), expected.blocks.size());
        for (const auto &block : expected.blocks) {
            const auto found =
                std::ranges::find_if(actual.blocks, [&](const auto &item) { return item.set == block.set && item.binding == block.binding; });
            ASSERT_NE(found, actual.blocks.end());
            CheckBlock(block, *found);
        }
        EXPECT_EQ(actual.resources.size(), expected.resources.size());
        for (const auto &resource : expected.resources) {
            const auto found = std::ranges::find_if(actual.resources,
                                                    [&](const auto &item) { return item.set == resource.set && item.binding == resource.binding; });
            ASSERT_NE(found, actual.resources.end()) << resource.name;
            CheckResource(resource, *found);
        }
    }

    class SharedPassHeaders : public testing::TestWithParam<ShaderTarget> {};
} // namespace

TEST_P(SharedPassHeaders, EveryDeclarationMatchesNativeCppLayoutAndResourceShape)
{
    const Declaration declarations[]{
        {.header = "GlobalBlock.h", .schema = shared::GetGlobalBlockReflection(), .expression = "mul(gGlobal.viewProj, gGlobal.cameraPos)"},
        {.header = "OpaquePass.h", .schema = shared::GetOpaquePassReflection(), .expression = "gOpaquePass.Misc"},
        {.header     = "ScenePass.h",
         .schema     = shared::GetScenePassReflection(),
         .expression = "mul(gScenePass.viewProjection, gScenePass.cameraPosition)"},
        {.header = "SceneBatch.h", .schema = shared::GetSceneBatchReflection(), .expression = "mul(gSceneBatch.world, gSceneBatch.tint)"},
        {.header     = "TextureToScreenPass.h",
         .schema     = shared::GetTextureToScreenPassReflection(),
         .expression = "gSourceTexture.Sample(gSourceSampler, uv) * gTextureToScreen.Misc"},
        {.header     = "SceneLightingPass.h",
         .schema     = shared::GetSceneLightingPassReflection(),
         .expression = "gSceneLighting.positions[uint(uv.x) % 8] + gShadowImage.Sample(gShadowSampler, uv)"},
        {.header     = "SceneEnvironmentPass.h",
         .schema     = shared::GetSceneEnvironmentPassReflection(),
         .expression = "gSceneEnvironment.environment + gDiffuseIrradiance.Sample(gEnvironmentSampler, uv) + "
                       "gSpecularEnvironment.Sample(gEnvironmentSampler, uv) + gBrdfIntegration.Sample(gBrdfSampler, uv)"},
        {.header = "SceneSurfaceBatch.h", .schema = shared::GetSceneSurfaceBatchReflection(), .expression = "gSceneSurface.surface"},
        {.header     = "ShadowPass.h",
         .schema     = shared::GetShadowPassReflection(),
         .expression = "mul(gShadowPass.lightViewProjection, float4(uv, 0, 1))"},
        {.header     = "ToneMapPass.h",
         .schema     = shared::GetToneMapPassReflection(),
         .expression = "gHdrColor.Sample(gHdrSampler, uv) * gToneMap.display"},
    };
    ShaderFileSystem files;
    files.AddSearchPath(AURORA_SHARED_SHADER_DIR);
    files.AddSearchPath(AURORA_SHADER_INCLUDE_DIR);
    ShaderCompilerSlang compiler;
    for (const auto &declaration : declarations) {
        SCOPED_TRACE(declaration.header);
        ShaderCompileDesc desc;
        desc.source = std::string("#include \"") + declaration.header + "\"\nfloat4 mainFS(float2 uv : TEXCOORD0) : SV_Target { return " +
                      declaration.expression + "; }\n";
        desc.entry      = "mainFS";
        desc.stage      = ShaderStageFlagBit::FS;
        desc.target     = GetParam();
        desc.fileSystem = &files;
        ShaderCompileResult result;
        ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
        CheckSchema(declaration.schema, result.reflection);
    }
}

TEST_P(SharedPassHeaders, PackagedSourcesCompileWithoutFilesystemSearchPaths)
{
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    ShaderCompilerSlang compiler;
    for (const char *file : {"SceneForward.slang", "SceneLit.slang", "SceneShadow.slang", "SceneToneMap.slang"}) {
        for (const bool vertex : {true, false}) {
            SCOPED_TRACE(file);
            SCOPED_TRACE(vertex);
            ShaderCompileDesc desc;
            ASSERT_TRUE(files.ReadFile(file, desc.source));
            desc.entry      = vertex ? "VSMain" : "FSMain";
            desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
            desc.target     = GetParam();
            desc.fileSystem = &files;
            ShaderCompileResult result;
            ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
        }
    }
}

TEST_P(SharedPassHeaders, FieldTypesAndResourceOnlyDeclarationsRetainTheirMetadata)
{
    ShaderFileSystem files;
    files.AddSearchPath(std::filesystem::path(__FILE__).parent_path().string());
    files.AddSearchPath(AURORA_SHADER_INCLUDE_DIR);
    ShaderCompileDesc desc;
    desc.source     = R"(
        #include "SharedShaderFixture.h"
        float4 mainFS(float2 uv : TEXCOORD0) : SV_Target {
            float4 value = gTest.directionWeight + gTest.values[uint(uv.x) % 2];
            value += float4(gTest.pairA, gTest.pairB) * (gTest.scalar + gTest.signedValue + gTest.unsignedValue);
            return gLayer.Sample(gSampling, uv) + (gTest.enabled ? mul(gTest.transform, value) : 0);
        }
    )";
    desc.entry      = "mainFS";
    desc.stage      = ShaderStageFlagBit::FS;
    desc.target     = GetParam();
    desc.fileSystem = &files;
    ShaderCompileResult result;
    ShaderCompilerSlang compiler;
    ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
    auto        schema    = shared::GetSharedTestReflection();
    const auto &resources = shared::GetSharedResourceTestReflection().resources;
    schema.resources.insert(schema.resources.end(), resources.begin(), resources.end());
    CheckSchema(schema, result.reflection);
}

TEST(SharedPassHeaderCache, SharedHelperChangesInvalidateTheSourceClosure)
{
    ShaderFileSystem files;
    RegisterSceneShaderSources(files);
    files.AddVirtualFile("nested/consumer.slang", "#include \"SceneLit.slang\"\n");
    uint64_t    before   = 0;
    uint64_t    after    = 0;
    uint64_t    repeated = 0;
    std::string source;
    std::string helper;
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, before, source));
    ASSERT_TRUE(files.ReadFile("aurora/shader/SharedShader.h", helper));
    files.AddVirtualFile("aurora/shader/SharedShader.h", helper + "\n#define SHARED_HEADER_REVISION 2\n");
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, after, source));
    EXPECT_NE(before, after);
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, repeated, source));
    EXPECT_EQ(after, repeated);
}

#if defined(_WIN32)
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SharedPassHeaders, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL));
#else
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SharedPassHeaders, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL));
#endif
