//
// Pipeline template smoke tests.
//

#include "AuroraTestHelper.h"

#include <aurora/pipeline/OpaquePass.h>
#include <aurora/pipeline/ScenePass.h>
#include <aurora/pipeline/TextureToScreenPass.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/CompiledGraph.h>
#include <aurora/rdg/RDGGraph.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, OpaquePassBuildRDG)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    OpaquePass opaque;
    opaque.SetExtent(1280, 720);
    opaque.OnSetup(device);
    opaque.BuildRDG(*graph);

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool found = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type != CompiledPassType::SCENE_RASTER) {
            continue;
        }
        found = true;
        const auto &p = std::get<SceneRasterPayload>(cpass.payload);
        ASSERT_EQ(p.queues.size(), 1u);
        EXPECT_STREQ(std::string(p.queues[0].name.GetStr()).c_str(), "opaque");
        EXPECT_EQ(p.queues[0].sortPolicy, QueueSortPolicy::FRONT_TO_BACK);
        EXPECT_EQ(p.renderArea.width, 1280u);
        EXPECT_EQ(p.renderArea.height, 720u);
    }
    EXPECT_TRUE(found);
}

TEST_F(AuroraVulkanTest, ScenePassBuildRDG)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    ScenePass pass;
    pass.SetExtent(1280, 720);
    pass.OnSetup(device);
    pass.BuildRDG(*graph);

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool found = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type != CompiledPassType::SCENE_RASTER) {
            continue;
        }
        found = true;
        const auto &p = std::get<SceneRasterPayload>(cpass.payload);
        ASSERT_EQ(p.colors.size(), 1u);
        EXPECT_EQ(p.colors[0].loadOp, LoadOp::CLEAR);
        EXPECT_EQ(p.colors[0].storeOp, StoreOp::STORE);
        ASSERT_NE(p.colors[0].image, nullptr);
        ASSERT_NE(p.depthStencil.image, nullptr);
        EXPECT_EQ(p.depthStencil.depthLoadOp, LoadOp::CLEAR);
        ASSERT_EQ(p.queues.size(), 1u);
        EXPECT_STREQ(std::string(p.queues[0].name.GetStr()).c_str(), "opaque");
        EXPECT_EQ(p.queues[0].sortPolicy, QueueSortPolicy::FRONT_TO_BACK);
        EXPECT_EQ(p.renderArea.width, 1280u);
        EXPECT_EQ(p.renderArea.height, 720u);
    }
    EXPECT_TRUE(found);
}

TEST_F(AuroraVulkanTest, TextureToScreenPassBuildRDG)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    RDGTextureDesc colorDesc{};
    colorDesc.extent      = {1280, 720, 1};
    colorDesc.format      = PixelFormat::RGBA16_SFLOAT;
    colorDesc.mipLevels   = 1;
    colorDesc.arrayLayers = 1;
    colorDesc.samples     = SampleCount::X1;
    colorDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;

    const auto hdr        = graph->CreateTexture(Name("HDRColor"), colorDesc);
    const auto backbuffer = graph->CreateTexture(Name("Backbuffer"), colorDesc);

    TextureToScreenPass pass;
    pass.SetInput(hdr);
    pass.SetTarget(backbuffer);
    pass.OnSetup(device);
    pass.BuildRDG(*graph);

    graph->MarkOfInterest(backbuffer);

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool found = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type != CompiledPassType::FULLSCREEN) {
            continue;
        }
        found = true;
        const auto &p = std::get<FullScreenPayload>(cpass.payload);
        ASSERT_EQ(p.colors.size(), 1u);
        EXPECT_EQ(p.colors[0].loadOp, LoadOp::DONT_CARE);
        EXPECT_EQ(p.colors[0].storeOp, StoreOp::STORE);
        ASSERT_NE(p.colors[0].image, nullptr);
    }
    EXPECT_TRUE(found);

    // input HDR texture is read as SRV by the fullscreen pass
    const auto &resources = graph->GetResources();
    ASSERT_LT(hdr.id, resources.size());
    bool hasSrvRead = false;
    for (const auto &rec : resources[hdr.id].accesses) {
        if (rec.access & AccessFlagBit::SRV) {
            hasSrvRead = true;
        }
    }
    EXPECT_TRUE(hasSrvRead);
}
