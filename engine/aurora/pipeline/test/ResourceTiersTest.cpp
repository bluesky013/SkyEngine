//
// Resource tiers tests: global resources, batch allocator, reflection validation.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/GlobalRenderResources.h>
#include <aurora/rdg/BatchAllocator.h>
#include <aurora/pipeline/ReflectionValidation.h>
#include <aurora/scene/SceneView.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>
#include <aurora/shader/ShaderCompilerSlang.h>
#include <aurora/rhi/Shader.h>
#include <core/archive/BinaryData.h>
#include <cstring>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    // Compile a dummy global shader carrying the set 0 layout
    // (ParameterBlock<GlobalParams> at [[vk::binding(0, 0)]]).
    Shader *MakeGlobalShader(Device *device)
    {
        const char *src = R"(
struct GlobalParams {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
};
[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal;

[shader("compute")]
[numthreads(1, 1, 1)]
void mainCS() {}
)";

        ShaderCompilerSlang compiler;
        ShaderCompileDesc   d{};
        d.source = src;
        d.entry  = "mainCS";
        d.stage  = ShaderStageFlagBit::CS;
        d.target = ShaderTarget::SPIRV;

        ShaderCompileResult r{};
        if (!compiler.Compile(d, r)) {
            return nullptr;
        }

        const size_t bytes = r.data.size() * sizeof(uint32_t);
        auto binary = CounterPtr<BinaryData>(new BinaryData(static_cast<uint32_t>(bytes)));
        std::memcpy(binary->Data(), r.data.data(), bytes);

        auto *provider       = new ShaderBinaryProvider();
        provider->binaryData = binary;

        ShaderFunction::Descriptor fnDesc = {};
        fnDesc.stage = ShaderStageFlagBit::CS;
        fnDesc.data  = CounterPtr<ShaderDataProvider>(provider);
        auto *cs = device->CreateShaderFunction(fnDesc);
        if (cs == nullptr) {
            return nullptr;
        }

        Shader::Descriptor shaderDesc = {};
        shaderDesc.cs         = cs;
        shaderDesc.reflection = &r.reflection;
        return device->CreateShader(shaderDesc);
    }

} // namespace

TEST_F(AuroraVulkanTest, GlobalRenderResourcesInitAndUpdate)
{
    auto *device = GetDevice();

    auto globalShader = CounterPtr<Shader>(MakeGlobalShader(device));
    ASSERT_NE(globalShader.Get(), nullptr);

    GlobalRenderResources global;
    ASSERT_TRUE(global.Init(device, globalShader.Get()));
    ASSERT_NE(global.GetGlobalResourceGroup(), nullptr);

    SceneView view;
    view.SetViewMatrix(Matrix4::Identity());
    view.SetProjectionMatrix(Matrix4::Identity());

    global.UpdateView(view, 1.5f); // must not crash; writes mapped UBO

    // wire into graph -> compiled graph carries the global RG
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);
    graph->SetGlobalResourceGroup(global.GetGlobalResourceGroup());

    const auto tex = graph->CreateTexture(Name("color"),
        [] { RDGTextureDesc d{}; d.extent = {16, 16, 1}; d.usage = ImageUsageFlagBit::RENDER_TARGET; return d; }());
    graph->AddSceneRasterPass(Name("p"),
        [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, tex, LoadOp::CLEAR, StoreOp::STORE); });
    graph->MarkOfInterest(tex);
    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);
    EXPECT_EQ(cg->globalResourceGroup, global.GetGlobalResourceGroup());
}

TEST_F(AuroraVulkanTest, BatchAllocatorAllocWriteReset)
{
    auto *device = GetDevice();

    BatchAllocator batch;
    ASSERT_TRUE(batch.Init(device, 4096));

    const uint32_t o0 = batch.Allocate(64);
    EXPECT_EQ(o0, 0u);

    const uint32_t o1 = batch.Allocate(64);
    EXPECT_EQ(o1, 256u); // 256B aligned

    const float value = 7.5f;
    batch.Write(o1, &value, sizeof(value));
    EXPECT_EQ(batch.GetUsedBytes(), 256u + 64u);

    // exhaustion
    EXPECT_EQ(batch.Allocate(8192), UINT32_MAX);

    batch.Reset();
    EXPECT_EQ(batch.GetUsedBytes(), 0u);
    EXPECT_EQ(batch.Allocate(64), 0u);
}

TEST_F(AuroraVulkanTest, BatchDynamicOffsetPassThrough)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    const auto tex = graph->CreateTexture(Name("color"),
        [] { RDGTextureDesc d{}; d.extent = {16, 16, 1}; d.usage = ImageUsageFlagBit::RENDER_TARGET; return d; }());

    DrawItem item{};
    item.batchDynamicOffset = 512;
    item.args.indexCount = 6;

    graph->AddSceneRasterPass(Name("p"),
        [&](SceneRasterPassBuilder &b) {
            b.ColorAttachment(0, tex, LoadOp::CLEAR, StoreOp::STORE);
            b.AddDrawItem(item);
        });
    graph->MarkOfInterest(tex);
    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);
    ASSERT_EQ(cg->passes.size(), 1u);

    const auto &p = std::get<SceneRasterPayload>(cg->passes[0].payload);
    ASSERT_EQ(p.queues.size(), 1u);
    ASSERT_EQ(p.queues[0].items.size(), 1u);
    EXPECT_EQ(p.queues[0].items[0].batchDynamicOffset, 512u);
}

TEST_F(AuroraVulkanTest, ReflectionValidationMatch)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.kind      = RgBlockKind::CBUFFER;
    desc.fields    = {{RgFieldType::MAT4, Name("ViewProj")}};

    ShaderReflection refl{};
    ShaderResource res{};
    res.name    = "Global";
    res.set     = 0;
    res.binding = 0;
    res.type    = ShaderResourceType::UNIFORM_BUFFER;
    refl.resources.push_back(res);

    EXPECT_TRUE(ValidateBlockAgainstReflection(desc, refl).empty());
}

TEST_F(AuroraVulkanTest, ReflectionValidationMismatch)
{
    RgBlockDesc desc{};
    desc.set       = 0;
    desc.binding   = 0;
    desc.blockName = Name("Global");
    desc.kind      = RgBlockKind::CBUFFER;

    ShaderReflection refl{};
    ShaderResource res{};
    res.name    = "Global";
    res.set     = 0;
    res.binding = 0;
    res.type    = ShaderResourceType::SAMPLED_IMAGE; // wrong type
    refl.resources.push_back(res);

    EXPECT_FALSE(ValidateBlockAgainstReflection(desc, refl).empty());

    // not present at all
    ShaderReflection empty{};
    EXPECT_FALSE(ValidateBlockAgainstReflection(desc, empty).empty());
}

TEST_F(AuroraVulkanTest, GeneratedBlockDescMatchesReflection)
{
    const char *shader = R"(
struct GlobalParams {
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4 cameraPos;
};
[[vk::binding(0, 0)]] ParameterBlock<GlobalParams> gGlobal;

[shader("fragment")]
float4 mainFS() : SV_Target { return gGlobal.cameraPos; }
)";

    ShaderCompilerSlang compiler;
    ShaderCompileDesc d{};
    d.source = shader;
    d.entry  = "mainFS";
    d.stage  = ShaderStageFlagBit::FS;
    d.target = ShaderTarget::SPIRV;

    ShaderCompileResult r{};
    ASSERT_TRUE(compiler.Compile(d, r)) << r.errorInfo;

    // the generated RgBlockDesc (from GlobalBlock.slang) must match the
    // shader reflection of the same block
    EXPECT_TRUE(ValidateBlockAgainstReflection(GlobalRenderResources::GetGlobalBlockDesc(),
                                               r.reflection).empty());
}
