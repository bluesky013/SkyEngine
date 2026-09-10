//
// Resource tiers tests: global resources, batch allocator, reflection validation.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/rg/GlobalRenderResources.h>
#include <aurora/pipeline/rg/BatchAllocator.h>
#include <aurora/pipeline/rg/ReflectionValidation.h>
#include <aurora/scene/SceneView.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, GlobalRenderResourcesInitAndUpdate)
{
    auto *device = GetDevice();

    GlobalRenderResources global;
    ASSERT_TRUE(global.Init(device));
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
