//
// Resource tiers tests: global resources, batch allocator, reflection validation.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/GlobalRenderResources.h>
#include <aurora/pipeline/BatchPackWriter.h>
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

    // Compile a shader carrying a ParameterBlock, then mark its uniform block
    // as UNIFORM_BUFFER_DYNAMIC in the reflection (Vulkan SPIR-V is identical
    // for static and dynamic UBOs; the static/dynamic distinction is descriptor
    // set layout side). Returns the created Shader and fills set/binding/blockSize.
    Shader *MakeDynamicBatchShader(Device *device, uint32_t &set, uint32_t &binding, uint32_t &blockSize)
    {
        const char *src = R"(
struct BatchParams {
    float4x4 model;
};
ParameterBlock<BatchParams> gBatch;

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

        set = 0;
        binding = 0;
        blockSize = 0;
        for (auto &res : r.reflection.resources) {
            if (res.type == ShaderResourceType::UNIFORM_BUFFER) {
                res.type = ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                set      = res.set;
                binding  = res.binding;
            }
        }
        for (const auto &block : r.reflection.blocks) {
            if (block.set == set && block.binding == binding) {
                blockSize = block.size;
            }
        }
        if (blockSize == 0) {
            blockSize = 64; // sizeof(float4x4)
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

    const uint32_t align = device->GetCapability().minUniformBufferOffsetAlignment;
    ASSERT_GE(align, 1u);

    BatchAllocator batch;
    ASSERT_TRUE(batch.Init(device, 4096));

    const uint32_t o0 = batch.Allocate(64);
    EXPECT_EQ(o0, 0u);

    const uint32_t o1 = batch.Allocate(64);
    EXPECT_EQ(o1, align); // aligned to device minUniformBufferOffsetAlignment

    const float value = 7.5f;
    batch.Write(o1, &value, sizeof(value));
    EXPECT_EQ(batch.GetUsedBytes(), align + 64u);

    // exhaustion
    EXPECT_EQ(batch.Allocate(8192), UINT32_MAX);

    batch.Reset();
    EXPECT_EQ(batch.GetUsedBytes(), 0u);
    EXPECT_EQ(batch.Allocate(64), 0u);
}

TEST_F(AuroraVulkanTest, BatchAllocatorAlignmentFromCaps)
{
    auto *device = GetDevice();

    const uint32_t align = device->GetCapability().minUniformBufferOffsetAlignment;
    ASSERT_GT(align, 0u);

    BatchAllocator batch;
    ASSERT_TRUE(batch.Init(device, 4096));

    // consecutive allocations must be multiples of the reported alignment,
    // with the gap equal to that alignment (not a hardcoded 256)
    const uint32_t o0 = batch.Allocate(1);
    const uint32_t o1 = batch.Allocate(1);
    EXPECT_EQ(o0 % align, 0u);
    EXPECT_EQ(o1 % align, 0u);
    EXPECT_EQ(o1 - o0, align);
}

TEST_F(AuroraVulkanTest, BatchPackWriterPackReturnsOffset)
{
    auto *device = GetDevice();

    BatchAllocator batch;
    ASSERT_TRUE(batch.Init(device, 4096));

    struct PerObject {
        float value[4];
    };
    PerObject a{};
    a.value[0] = 1.0f;

    BatchPackWriter writer(batch);
    const uint32_t o0 = writer.Pack(a);
    const uint32_t o1 = writer.Pack(a);

    EXPECT_EQ(o0, 0u);
    EXPECT_EQ(o1, device->GetCapability().minUniformBufferOffsetAlignment);
}

TEST_F(AuroraVulkanTest, BatchDynamicUboStableBinding)
{
    auto *device = GetDevice();

    uint32_t set = 0, binding = 0, blockSize = 0;
    auto batchShader = CounterPtr<Shader>(MakeDynamicBatchShader(device, set, binding, blockSize));
    ASSERT_NE(batchShader.Get(), nullptr);
    ASSERT_GT(blockSize, 0u);

    // batch RG: descriptor set layout derives a UNIFORM_BUFFER_DYNAMIC binding
    ResourceGroup::Descriptor rgDesc{};
    rgDesc.shader = batchShader.Get();
    rgDesc.set    = set;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(rgDesc));
    ASSERT_NE(group.Get(), nullptr);

    // pack buffer (host-visible; one frame worth of packed blocks)
    Buffer::Descriptor bufDesc{};
    bufDesc.size   = blockSize * 4;
    bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
    bufDesc.memory = MemoryType::CPU_TO_GPU;
    auto buffer = CounterPtr<Buffer>(device->CreateBuffer(bufDesc));
    ASSERT_NE(buffer.Get(), nullptr);

    // stable binding: offset=0, range=blockSize, written once per frame
    ResourceUpdateInfo write{};
    write.binding      = binding;
    write.kind         = ResourceWriteKind::BUFFER;
    write.buffer       = buffer.Get();
    write.bufferOffset = 0;
    write.bufferRange  = blockSize;
    group->Update({write}); // must not assert
    group->Update({write}); // a second frame bind with the same explicit range also succeeds
}

TEST_F(AuroraVulkanTest, BatchDynamicUboRangeZeroRejected)
{
    auto *device = GetDevice();

    uint32_t set = 0, binding = 0, blockSize = 0;
    auto batchShader = CounterPtr<Shader>(MakeDynamicBatchShader(device, set, binding, blockSize));
    ASSERT_NE(batchShader.Get(), nullptr);

    ResourceGroup::Descriptor rgDesc{};
    rgDesc.shader = batchShader.Get();
    rgDesc.set    = set;
    auto group = CounterPtr<ResourceGroup>(device->CreateResourceGroup(rgDesc));
    ASSERT_NE(group.Get(), nullptr);

    Buffer::Descriptor bufDesc{};
    bufDesc.size   = 1024;
    bufDesc.usage  = BufferUsageFlagBit::UNIFORM;
    bufDesc.memory = MemoryType::CPU_TO_GPU;
    auto buffer = CounterPtr<Buffer>(device->CreateBuffer(bufDesc));
    ASSERT_NE(buffer.Get(), nullptr);

    ResourceUpdateInfo write{};
    write.binding     = binding;
    write.kind        = ResourceWriteKind::BUFFER;
    write.buffer      = buffer.Get();
    write.bufferRange = 0; // bug: dynamic UBO requires an explicit range

#if defined(_DEBUG)
    EXPECT_DEATH({ group->Update({write}); }, "requires explicit bufferRange");
#else
    group->Update({write}); // release: logs an error, must not crash
#endif
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
