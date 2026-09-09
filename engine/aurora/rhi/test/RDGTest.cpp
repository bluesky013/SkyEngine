//
// Aurora RDG tests.
//
// The full GPU readback tests (TwoPassChain / ComputeToGraphics / ImportSwapChain)
// require shader/pipeline and swapchain infrastructure that is outside this change's
// scope; they are deferred to a follow-up.
//

#include "AuroraTestHelper.h"

#include <core/name/Name.h>
#include <core/memory/FrameAllocator.h>

#include <aurora/rdg/RDGHandles.h>
#include <aurora/rdg/RDGTypes.h>
#include <aurora/rdg/RenderGraph.h>

#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/SubmitInfo.h>

#include <type_traits>
#include <unordered_map>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

// ---- 5.8 handle type safety (compile-time) ----
static_assert(!std::is_same_v<RDGTextureHandle, RDGBufferHandle>);
static_assert(!std::is_convertible_v<RDGTextureHandle, RDGBufferHandle>);
static_assert(!std::is_convertible_v<RDGBufferHandle, RDGTextureHandle>);

// ---- 5.9 handle equality + hash ----
TEST(RDGHandleTest, Equality)
{
    const RDGTextureHandle a{1};
    const RDGTextureHandle b{1};
    const RDGTextureHandle c{2};
    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_TRUE(a.IsValid());
    EXPECT_FALSE(RDGTextureHandle{}.IsValid());
}

TEST(RDGHandleTest, Hash)
{
    std::unordered_map<RDGTextureHandle, int> map;
    map[RDGTextureHandle{1}] = 42;
    EXPECT_EQ(map[RDGTextureHandle{1}], 42);
    EXPECT_EQ(map.count(RDGTextureHandle{2}), 0u);
}

namespace {

    RDGTextureDesc MakeColorDesc(uint32_t w, uint32_t h)
    {
        RDGTextureDesc desc;
        desc.format       = PixelFormat::RGBA8_UNORM;
        desc.extent       = {w, h, 1};
        desc.mipLevels    = 1;
        desc.arrayLayers  = 1;
        desc.samples      = SampleCount::X1;
        desc.usage        = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
        desc.residency    = ResourceResidency::TRANSIENT;
        return desc;
    }

} // namespace

using RDGTestVulkan = AuroraVulkanTest;

// ---- 5.1 single raster pass clear, executed and submitted ----
TEST_F(RDGTestVulkan, SinglePassClear)
{
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(device, nullptr);

    Image::Descriptor imgDesc{};
    imgDesc.imageType   = ImageType::IMAGE_2D;
    imgDesc.format      = PixelFormat::RGBA8_UNORM;
    imgDesc.extent      = {32, 32, 1};
    imgDesc.mipLevels   = 1;
    imgDesc.arrayLayers = 1;
    imgDesc.samples     = SampleCount::X1;
    imgDesc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    imgDesc.memory      = MemoryType::GPU_ONLY;
    CounterPtr<Image> image(device->CreateImage(imgDesc));
    ASSERT_NE(image.Get(), nullptr);

    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);
    const auto bb = graph->Import(Name("backbuffer"), image, AccessFlagBit::NONE);
    graph->AddSceneRasterPass(Name("clear"),
        [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, bb, LoadOp::CLEAR, StoreOp::STORE); },
        [](GraphicsEncoder &, RDGContext &) {});
    graph->Compile();

    auto pool   = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto fence  = MakeFence(device);
    auto *cmdBuf = pool->Allocate();

    cmdBuf->Begin();
    graph->Execute(cmdBuf);
    cmdBuf->End();

    SubmitInfo submit{};
    submit.commandBuffers.push_back(cmdBuf);
    submit.fence = fence.Get();
    queue->Submit(submit);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

// ---- 5.4 pass culling ----
TEST_F(RDGTestVulkan, PassCulling)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    const auto deadTex = graph->CreateTexture(Name("dead"), MakeColorDesc(16, 16));
    const auto liveTex = graph->CreateTexture(Name("live"), MakeColorDesc(16, 16));

    graph->AddSceneRasterPass(Name("dead-pass"),
        [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, deadTex, LoadOp::CLEAR, StoreOp::STORE); },
        [](GraphicsEncoder &, RDGContext &) {});

    graph->AddSceneRasterPass(Name("live-pass"),
        [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, liveTex, LoadOp::CLEAR, StoreOp::STORE); },
        [](GraphicsEncoder &, RDGContext &) {});

    graph->MarkOfInterest(liveTex);
    graph->Compile();

    const auto &passes = graph->GetPasses();
    ASSERT_EQ(passes.size(), 2u);
    EXPECT_FALSE(passes[0].live);
    EXPECT_TRUE(passes[1].live);

    // FrameAllocator should have non-zero arena usage after graph construction
    // (TransientStdAllocator calls Arena() directly, not FrameAllocator::Allocate)
    EXPECT_GT(frameAlloc.Arena().GetCurrentUsedSize(), 0u);
}

// ---- 5.5 transient aliasing (4 non-overlapping resources -> 1 backing image) ----
TEST_F(RDGTestVulkan, TransientAliasing)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    for (int i = 0; i < 4; ++i) {
        const auto tex = graph->CreateTexture(Name(("t" + std::to_string(i)).c_str()), MakeColorDesc(1080, 1080));
        graph->AddSceneRasterPass(Name(("p" + std::to_string(i)).c_str()),
            [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, tex, LoadOp::CLEAR, StoreOp::STORE); },
            [](GraphicsEncoder &, RDGContext &) {});
        graph->MarkOfInterest(tex);
    }

    graph->Compile();

    const auto &stats = graph->GetPoolStats();
    EXPECT_EQ(stats.imageMisses, 1u);
    EXPECT_EQ(stats.imageHits, 3u);
}

// ---- 5.6 transient cross-frame (repeated Compile reuses the pool) ----
TEST_F(RDGTestVulkan, TransientCrossFrame)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    for (int i = 0; i < 4; ++i) {
        const auto tex = graph->CreateTexture(Name(("t" + std::to_string(i)).c_str()), MakeColorDesc(128, 128));
        graph->AddSceneRasterPass(Name(("p" + std::to_string(i)).c_str()),
            [&](SceneRasterPassBuilder &b) { b.ColorAttachment(0, tex, LoadOp::CLEAR, StoreOp::STORE); },
            [](GraphicsEncoder &, RDGContext &) {});
        graph->MarkOfInterest(tex);
    }

    for (int frame = 0; frame < 5; ++frame) {
        graph->Compile();
    }

    const auto &stats = graph->GetPoolStats();
    EXPECT_EQ(stats.imageMisses, 1u);
    EXPECT_EQ(stats.imageHits, 3u + 4u * 4u);
}

// ---- pass structure: variant payload types ----
TEST_F(RDGTestVulkan, PassStructureVariant)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    // dependency chain: scene -> {fs, compute} -> copyblit -> custom -> present
    const auto texScene = graph->CreateTexture(Name("scene"), MakeColorDesc(64, 64));
    const auto texFs    = graph->CreateTexture(Name("fsOut"), MakeColorDesc(64, 64));
    const auto texComp  = graph->CreateTexture(Name("compOut"), MakeColorDesc(64, 64));
    const auto texCopy  = graph->CreateTexture(Name("copyOut"), MakeColorDesc(64, 64));
    const auto colorTex = graph->CreateTexture(Name("color"), MakeColorDesc(64, 64));

    // scene raster with draw items
    DrawItem item0{};
    DrawItem item1{};
    item1.args.indexCount = 6;
    graph->AddSceneRasterPass(Name("scene"),
        [&](SceneRasterPassBuilder &b) {
            b.ColorAttachment(0, texScene, LoadOp::CLEAR, StoreOp::STORE);
            b.AddDrawItem(item0);
            b.AddDrawItem(item1);
        });

    // fullscreen: read scene, write fsOut
    graph->AddFullScreenPass(Name("fs"),
        [&](FullScreenPassBuilder &b) {
            b.SetInputSRV(texScene);
            b.SetTarget(texFs, LoadOp::CLEAR, StoreOp::STORE);
        });

    // compute: read scene, write compOut
    graph->AddComputePass(Name("comp"),
        [&](ComputePassBuilder &b) {
            b.Read(texScene, AccessFlagBit::SRV);
            b.Write(texComp, AccessFlagBit::UAV);
        });

    // copyblit: compOut -> copyOut
    graph->AddCopyBlitPass(Name("copy"),
        [&](CopyBlitPassBuilder &b) { b.Src(texComp).Dst(texCopy); });

    // custom: read copyOut, write color
    graph->AddCustomPass(Name("custom"),
        [&](CustomPassBuilder &b) {
            b.Read(texCopy, AccessFlagBit::SRV);
            b.Write(colorTex, AccessFlagBit::UAV);
        },
        [](RDGContext &, CommandBuffer &) {});

    // present: read fsOut + color
    graph->AddPresentPass(Name("present"),
        [&](PresentPassBuilder &b) {
            b.SetSource(texFs);
            b.SetSource(colorTex);
        });

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool sawSceneRaster = false, sawFullScreen = false, sawCompute = false,
         sawCopyBlit = false, sawPresent = false, sawCustom = false;
    for (const auto &cpass : cg->passes) {
        switch (cpass.type) {
        case CompiledPassType::SCENE_RASTER: {
            sawSceneRaster = true;
            const auto &p = std::get<SceneRasterPayload>(cpass.payload);
            EXPECT_EQ(p.items.size(), 2u);
            EXPECT_EQ(p.items[1].args.indexCount, 6u);
            break;
        }
        case CompiledPassType::FULLSCREEN:
            sawFullScreen = true;
            EXPECT_TRUE(std::holds_alternative<FullScreenPayload>(cpass.payload));
            break;
        case CompiledPassType::COMPUTE:
            sawCompute = true;
            EXPECT_TRUE(std::holds_alternative<ComputePayload>(cpass.payload));
            break;
        case CompiledPassType::COPYBLIT:
            sawCopyBlit = true;
            EXPECT_TRUE(std::holds_alternative<CopyBlitPayload>(cpass.payload));
            break;
        case CompiledPassType::PRESENT:
            sawPresent = true;
            EXPECT_TRUE(std::holds_alternative<PresentPayload>(cpass.payload));
            break;
        case CompiledPassType::CUSTOM:
            sawCustom = true;
            EXPECT_TRUE(std::holds_alternative<CustomPayload>(cpass.payload));
            break;
        }
    }
    EXPECT_TRUE(sawSceneRaster);
    EXPECT_TRUE(sawFullScreen);
    EXPECT_TRUE(sawCompute);
    EXPECT_TRUE(sawCopyBlit);
    EXPECT_TRUE(sawPresent);
    EXPECT_TRUE(sawCustom);
}
