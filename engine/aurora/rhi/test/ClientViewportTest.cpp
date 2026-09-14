//
// ClientViewport / BindViewport / frame context smoke tests.
//

#include "AuroraTestHelper.h"

#include <aurora/rdg/CompiledGraph.h>
#include <aurora/rdg/RenderGraph.h>
#include <aurora/rdg/RenderViewport.h>
#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/RenderDeviceExclusive.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/Semaphore.h>
#include <core/name/Name.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

    class FakeViewport : public RenderViewport {
    public:
        explicit FakeViewport(Image *img, bool acquireSucceeds = true)
            : mBackbuffer(img)
            , mAcquireSucceeds(acquireSucceeds)
        {
        }

        bool Begin() override { return mBackbuffer != nullptr; }
        bool Acquire() override { return mAcquireSucceeds && mBackbuffer != nullptr; }
        void Release() override {}

        Image *GetBackbuffer() const override { return mBackbuffer; }
        PixelFormat GetFormat() const override { return PixelFormat::RGBA8_UNORM; }
        Extent2D GetExtent() const override { return {64, 64}; }
        const Name &GetName() const override { return mName; }

        sky::aurora::Semaphore *GetAcquireSemaphore() const override { return nullptr; }
        sky::aurora::Semaphore *GetRenderDoneSemaphore() const override { return nullptr; }

    private:
        Image *mBackbuffer;
        bool   mAcquireSucceeds;
        Name   mName = Name("FakeViewport");
    };

    ImagePtr MakeTestImage(Device *device)
    {
        Image::Descriptor desc{};
        desc.format      = PixelFormat::RGBA8_UNORM;
        desc.extent      = {64, 64, 1};
        desc.mipLevels   = 1;
        desc.arrayLayers = 1;
        desc.samples     = SampleCount::X1;
        desc.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::SAMPLED;
        return CounterPtr<Image>(device->CreateImage(desc));
    }

} // namespace

TEST_F(AuroraVulkanTest, BindViewportPresentIntegration)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    ImagePtr image = MakeTestImage(device);
    FakeViewport viewport(image.Get());

    const auto bb = graph->BindViewport(Name("backbuffer"), &viewport);

    graph->AddFullScreenPass(Name("composite"), [&](FullScreenPassBuilder &b) {
        b.SetTarget(bb, LoadOp::CLEAR, StoreOp::STORE);
    });
    graph->AddPresentPass(Name("present"), [&](PresentPassBuilder &b) {
        b.SetSource(bb);
    });

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool sawComposite = false;
    bool sawPresent   = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type == CompiledPassType::FULLSCREEN) {
            sawComposite = true;
        } else if (cpass.type == CompiledPassType::PRESENT) {
            sawPresent = true;
        }
    }
    EXPECT_TRUE(sawComposite); // backbuffer is a culling seed
    EXPECT_TRUE(sawPresent);
}

TEST_F(AuroraVulkanTest, BindViewportAcquireFailNoSeed)
{
    auto *device = GetDevice();
    FrameAllocator frameAlloc;
    auto graph = RenderGraph::Build(device, frameAlloc);

    ImagePtr image = MakeTestImage(device);
    FakeViewport viewport(image.Get(), /*acquireSucceeds=*/false);

    const auto bb = graph->BindViewport(Name("backbuffer"), &viewport);

    // no present pass → the only thing keeping the writer alive is the seed
    graph->AddFullScreenPass(Name("composite"), [&](FullScreenPassBuilder &b) {
        b.SetTarget(bb, LoadOp::CLEAR, StoreOp::STORE);
    });

    graph->Compile();

    const auto *cg = graph->GetCompiledGraph();
    ASSERT_NE(cg, nullptr);

    bool sawComposite = false;
    for (const auto &cpass : cg->passes) {
        if (cpass.type == CompiledPassType::FULLSCREEN) {
            sawComposite = true;
        }
    }
    EXPECT_FALSE(sawComposite); // acquire failed → not a seed → writer culled
}

TEST_F(AuroraVulkanTest, DeviceFrameContextFenceRing)
{
    auto *device = GetDevice();

    DeviceFrameContextInitInfo info{};
    info.inflightNum = 2;
    info.parallelNum = 1;
    std::unique_ptr<DeviceFrameContext> ctx(device->CreateFrameContext(info));
    ASSERT_NE(ctx, nullptr);

    Fence *f0 = ctx->GetFrameFence();
    ASSERT_NE(f0, nullptr);

    ctx->BeginFrame(); // waits + resets fence[0] (signaled on creation)
    ctx->EndFrame();   // ++mFrameIndex → 1

    Fence *f1 = ctx->GetFrameFence();
    ASSERT_NE(f1, nullptr);
    EXPECT_NE(f0, f1); // different slot for the next frame
}

TEST_F(AuroraVulkanTest, RenderDeviceExclusiveViewportBeginEnd)
{
    auto *re = RenderDeviceExclusive::Get();
    ASSERT_NE(re, nullptr);

    ImagePtr image = MakeTestImage(GetDevice());
    FakeViewport viewport(image.Get());

    re->BeginViewport(&viewport); // SKY_ASSERT(mCurrentViewport == nullptr) must not fire
    re->EndViewport();
    SUCCEED();
}

TEST_F(AuroraVulkanTest, ClientViewportUninitializedSafe)
{
    ClientViewport viewport(Name("uninit"));

    // Without Init() the viewport must be safe: no crash, no present.
    EXPECT_FALSE(viewport.Begin());
    EXPECT_FALSE(viewport.Acquire());
    viewport.Release(); // no-op
    EXPECT_EQ(viewport.GetBackbuffer(), nullptr);
}
