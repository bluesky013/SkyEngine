#include "GlobalBlock.h"
#include "MaterialBlock.h"
#include "OpaquePass.h"
#include "SceneBatch.h"
#include "SceneEnvironmentPass.h"
#include "SceneLightingPass.h"
#include "ScenePass.h"
#include "SceneSurfaceBatch.h"
#include "SkinningBatch.h"
#include "SelectionMaskPass.h"
#include "SelectionOutlinePass.h"
#include "ShaderSourcesFixture.h"
#include "ShadowPass.h"
#include "SharedShaderFixture.h"
#include "TextureToScreenPass.h"
#include "ToneMapPass.h"
#include <algorithm>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/shader/ShaderFileSystem.h>
#include <aurora/shader/ShaderResolver.h>
#include <aurora/shader/ShaderTechnique.h>
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
        if (expected.type == ShaderResourceType::SAMPLED_IMAGE || expected.type == ShaderResourceType::STORAGE_IMAGE)
            EXPECT_EQ(actual.texture, expected.texture);
        EXPECT_EQ(actual.sampler, expected.sampler);
        EXPECT_EQ(actual.bufferKind, expected.bufferKind);
        EXPECT_EQ(actual.elementStride, expected.elementStride);
        EXPECT_EQ(actual.access, expected.access);
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
        {.header = "GlobalBlock.h", .schema = shared::GetGlobalBlockReflection(), .expression = "gGlobal.frameTime"},
        {.header = "MaterialBlock.h", .schema = shared::GetMaterialBlockReflection(), .expression = "gMaterial.tint"},
        {.header     = "OpaquePass.h",
         .schema     = shared::GetOpaquePassReflection(),
         .expression = "mul(gOpaquePass.viewProjection, gOpaquePass.cameraPosition)"},
        {.header     = "ScenePass.h",
         .schema     = shared::GetScenePassReflection(),
         .expression = "mul(gScenePass.viewProjection, gScenePass.cameraPosition)"},
        {.header     = "SceneBatch.h",
         .schema     = shared::GetSceneBatchReflection(),
         .expression = "mul(gSceneBatch.world, mul(gSceneBatch.world, float4(1, 1, 1, 1)))"},
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
        {.header = "SkinningBatch.h", .schema = shared::GetSkinningBatchReflection(), .expression = "gSkinningPalette[uint(uv.x) % 8]"},
        {.header     = "ShadowPass.h",
         .schema     = shared::GetShadowPassReflection(),
         .expression = "mul(gShadowPass.lightViewProjection, float4(uv, 0, 1))"},
        {.header     = "ToneMapPass.h",
         .schema     = shared::GetToneMapPassReflection(),
         .expression = "gHdrColor.Sample(gHdrSampler, uv) * gToneMap.display"},
        {.header     = "SelectionMaskPass.h",
         .schema     = shared::GetSelectionMaskPassReflection(),
         .expression = "mul(gSelectionMask.viewProjection, float4(uv, 0, 1)) * gSceneDepth.Load(int3(0, 0, 0))"},
        {.header     = "SelectionOutlinePass.h",
         .schema     = shared::GetSelectionOutlinePassReflection(),
         .expression = "gSelectionOutline.visible + gSelectionOutline.occluded + gSelectionOutline.metrics + gSelectionImage.Load(int3(0, 0, 0))"},
    };
    ShaderFileSystem files;
    files.AddSearchPath(AURORA_SHADER_ASSET_DIR);
    ShaderCompilerSlang compiler;
    for (const auto &declaration : declarations) {
        SCOPED_TRACE(declaration.header);
        ShaderCompileDesc desc;
        const auto        exported = std::filesystem::path(declaration.header).replace_extension(".slangh").generic_string();
        desc.source = std::string("#include <aurora/pipeline/") + exported + ">\nfloat4 mainFS(float2 uv : TEXCOORD0) : SV_Target { return " +
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
    ASSERT_TRUE(LoadTestShaderSources(files));
    ShaderCompilerSlang compiler;
    for (const char *file : {"aurora/materials/Unlit.slang", "aurora/materials/Lit.slang", "aurora/materials/Shadow.slang", "aurora/pipeline/ToneMap.slang",
                             "aurora/pipeline/SelectionMask.slang", "aurora/pipeline/SelectionOutline.slang"}) {
        const bool material = std::string_view(file).starts_with("aurora/materials/");
        for (const bool vertex : {true, false}) {
            for (const bool skinned : {false, true}) {
                if (!material && skinned)
                    continue;
                SCOPED_TRACE(file);
                SCOPED_TRACE(vertex);
                SCOPED_TRACE(skinned);
                ShaderCompileDesc desc;
                ASSERT_TRUE(files.ReadFile(file, desc.source));
                ShaderVariant variant;
                if (skinned)
                    variant.entries.push_back({Name("SKINNED"), 1});
                desc.sourcePath = file;
                desc.entry      = vertex ? "VSMain" : "FSMain";
                desc.stage      = vertex ? ShaderStageFlagBit::VS : ShaderStageFlagBit::FS;
                desc.target     = GetParam();
                desc.fileSystem = &files;
                desc.variant    = variant.entries.empty() ? nullptr : &variant;
                ShaderCompileResult result;
                ASSERT_TRUE(compiler.Compile(desc, result)) << result.errorInfo;
                if (vertex && skinned) {
                    const auto &inputs = result.reflection.vertexInputs;
                    const bool lit     = std::string_view(file).ends_with("Lit.slang");
                    ASSERT_EQ(inputs.size(), lit ? 4u : 3u);
                    EXPECT_EQ(inputs[0], (ShaderVertexInput{"POSITION", 0, 0, ShaderScalarType::FLOAT, 3}));
                    const auto joint = lit ? 2u : 1u;
                    EXPECT_EQ(inputs[joint], (ShaderVertexInput{"JOINTS", 0, joint, ShaderScalarType::UINT, 4}));
                    EXPECT_EQ(inputs[joint + 1], (ShaderVertexInput{"WEIGHTS", 0, joint + 1, ShaderScalarType::FLOAT, 4}));
                }
            }
        }
    }
}

TEST_P(SharedPassHeaders, FieldTypesAndResourceOnlyDeclarationsRetainTheirMetadata)
{
    ShaderFileSystem files;
    files.AddSearchPath(AURORA_TEST_SHADER_ASSET_DIR);
    ShaderCompileDesc desc;
    desc.source     = R"(
        #include <pipeline_test/SharedShaderFixture.slangh>
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
    ASSERT_TRUE(LoadTestShaderSources(files));
    files.AddVirtualFile("nested/consumer.slang", "#include <aurora/materials/Lit.slang>\n");
    uint64_t    before   = 0;
    uint64_t    after    = 0;
    uint64_t    repeated = 0;
    std::string source;
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, before, source));
    std::string helper;
    ASSERT_TRUE(files.ReadFile("aurora/pipeline/OpaquePass.slangh", helper));
    EXPECT_FALSE(files.AddVirtualFile("aurora/pipeline/OpaquePass.slangh", helper + "// edit"));
    ASSERT_TRUE(files.RemoveBundle("aurora"));
    NativeFileSystem                root{FilePath(SHADER_TEST_SOURCE_ROOT)};
    std::vector<ShaderSourceBundle> bundles;
    ASSERT_TRUE(LoadShaderSourceBundles(root, "aurora", bundles));
    auto &changed = bundles.front().files;
    auto  found   = std::ranges::find(changed, "aurora/pipeline/OpaquePass.slangh", &ShaderSourceFile::path);
    ASSERT_NE(found, changed.end());
    found->content += "\n// declaration revision\n";
    ASSERT_TRUE(files.AddBundles(bundles));
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, after, source));
    EXPECT_NE(before, after);
    ASSERT_TRUE(ShaderResolver::HashSourceClosure(files, "nested/consumer.slang", {}, repeated, source));
    EXPECT_EQ(after, repeated);
}

TEST_P(SharedPassHeaders, ExplicitTechniquesHaveArbitraryEntriesAndUnusedHeadersNeedNoBatch)
{
    struct CompilerScope {
        ShaderCompilerSlang compiler;
        IShaderCompiler    *previous = ShaderCompilerFactory::Get().GetCompiler();
        CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(&compiler);
        }
        ~CompilerScope()
        {
            ShaderCompilerFactory::Get().Register(previous);
        }
    } scope;
    ShaderFileSystem files;
    files.AddSearchPath(AURORA_SHADER_ASSET_DIR);
    files.AddVirtualFile("custom/Program.slang", R"(
        #include <aurora/pipeline/MaterialBlock.slangh>
        float4 DrawTriangle(uint id : SV_VertexID) : SV_Position { return float4(id == 0 ? -1 : 1, id == 2 ? 1 : -1, 0, 1); }
        float4 ShadeSurface() : SV_Target { return float4(0, 0, 1, 1); }
        [numthreads(2, 3, 1)] void DispatchTiles(uint3 id : SV_DispatchThreadID) {}
    )");
    files.AddVirtualFile("custom/Program.slang.json", R"({"entries":["DrawTriangle","ShadeSurface","DispatchTiles"],"variants":[]})");
    const auto      backend = GetParam() == ShaderTarget::SPIRV ? API::VULKAN : GetParam() == ShaderTarget::DXIL ? API::DX12 : API::METAL;
    ShaderTechnique graphics{{"custom/Program.slang", "DrawTriangle"}, {"custom/Program.slang", "ShadeSurface"}};
    const auto      raster = CompileShaderTechnique(files, backend, graphics);
    ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(raster)) << std::get<RendererError>(raster).message;
    EXPECT_TRUE(std::get<RenderTechniqueData>(raster).reflection.resources.empty());
    ASSERT_TRUE(std::get<RenderTechniqueData>(raster).vertex);
    EXPECT_EQ(std::get<RenderTechniqueData>(raster).vertex->identity.sourceEntry, "DrawTriangle");
    ShaderTechnique compute;
    compute.compute     = {"custom/Program.slang", "DispatchTiles"};
    const auto dispatch = CompileShaderTechnique(files, backend, compute);
    ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(dispatch)) << std::get<RendererError>(dispatch).message;
    const auto &code = std::get<RenderTechniqueData>(dispatch);
    EXPECT_TRUE(code.reflection.resources.empty());
    EXPECT_EQ(code.reflection.threadGroupSize[0], 2u);
    EXPECT_EQ(code.reflection.threadGroupSize[1], 3u);
    graphics.compute = compute.compute;
    EXPECT_TRUE(std::holds_alternative<RendererError>(CompileShaderTechnique(files, backend, graphics)));
    EXPECT_TRUE(std::holds_alternative<RendererError>(CompileShaderTechnique(files, backend, {})));
    compute.compute.entry.clear();
    EXPECT_TRUE(std::holds_alternative<RendererError>(CompileShaderTechnique(files, backend, compute)));
    files.AddVirtualFile("custom/Vertex.slang", R"(
        #ifdef FRAGMENT_ONLY
        #error Fragment values leaked into the vertex stage
        #endif
        float4 Position(uint id : SV_VertexID) : SV_Position { return float4(id, 0, 0, 1); }
    )");
    files.AddVirtualFile("custom/Fragment.slang", R"(
        #if FRAGMENT_ONLY != 1
        #error Fragment schema default was lost
        #endif
        float4 Surface() : SV_Target { return float4(0, 1, 0, 1); }
    )");
    files.AddVirtualFile("custom/Vertex.slang.json", R"({"variants":[]})");
    files.AddVirtualFile("custom/Fragment.slang.json", R"({"sources":[{"name":"material","width":1}],
        "variants":[{"key":"FRAGMENT_ONLY","source":"material","default":1}]})");
    ShaderTechniqueVariant selected;
    ShaderResolver         resolver(nullptr, nullptr, files, 0, 0);
    uint64_t               fingerprint = 0;
    std::string            error;
    ASSERT_TRUE(resolver.ResolveSchema("custom/Fragment.slang", static_cast<uint32_t>(GetParam()), selected.schema, fingerprint, &error));
    selected.macros.entries.push_back({Name("FRAGMENT_ONLY"), 1});
    ShaderTechnique separate{{"custom/Vertex.slang", "Position"}, {"custom/Fragment.slang", "Surface"}};
    const auto      stages = CompileShaderTechnique(files, backend, separate, nullptr, &selected);
    ASSERT_TRUE(std::holds_alternative<RenderTechniqueData>(stages)) << std::get<RendererError>(stages).message;
}

#if defined(_WIN32)
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SharedPassHeaders, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL, ShaderTarget::DXIL));
#else
INSTANTIATE_TEST_SUITE_P(ShaderTargets, SharedPassHeaders, testing::Values(ShaderTarget::SPIRV, ShaderTarget::MSL));
#endif
