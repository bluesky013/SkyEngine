//
// Aurora barrier end-to-end tests via CommandBuffer::PipelineBarrier.
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/Barrier.h>
#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/SubmitInfo.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

namespace {

CounterPtr<Image> MakeColorRT(Device *device, uint32_t w, uint32_t h)
{
    Image::Descriptor d = {};
    d.imageType   = ImageType::IMAGE_2D;
    d.format      = PixelFormat::RGBA8_UNORM;
    d.extent      = {w, h, 1};
    d.mipLevels   = 1;
    d.arrayLayers = 1;
    d.samples     = SampleCount::X1;
    d.usage       = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC;
    d.memory      = MemoryType::GPU_ONLY;
    return CounterPtr<Image>(device->CreateImage(d));
}

BarrierInfo MakeImageTransition(Image *img, AccessFlags srcAccess, AccessFlags dstAccess,
                                PipelineStageFlags srcStage, PipelineStageFlags dstStage)
{
    ImageBarrierInfo ib{};
    ib.image     = img;
    ib.srcAccess = srcAccess;
    ib.dstAccess = dstAccess;
    ib.oldLayout = InferLayoutForAccess(srcAccess);
    ib.newLayout = InferLayoutForAccess(dstAccess);
    ib.subRange.aspectMask = AspectFlagBit::COLOR_BIT;

    BarrierInfo info{};
    info.srcStage = srcStage;
    info.dstStage = dstStage;
    info.imageBarriers.push_back(ib);
    return info;
}

} // namespace

using BarrierTestVulkan = AuroraVulkanTest;

TEST_F(BarrierTestVulkan, ImageTransitionBeforeRendering)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto image = MakeColorRT(device, 32, 32);
    ASSERT_NE(image.Get(), nullptr);

    auto pool   = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto fence  = MakeFence(device);
    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    auto *cmdBuf = pool->Allocate();

    cmdBuf->Begin();
    {
        // UNDEFINED → COLOR_ATTACHMENT
        BarrierInfo pre = MakeImageTransition(image.Get(),
            AccessFlagBit::NONE, AccessFlagBit::COLOR_WRITE,
            PipelineStageBit::TOP, PipelineStageBit::COLOR_OUTPUT);
        cmdBuf->PipelineBarrier(pre);

        auto enc = cmdBuf->CreateGraphicsEncoder();
        RenderingInfo info{};
        info.renderArea = {{0, 0}, {32, 32}};
        info.numColors  = 1;
        info.colors[0].image   = image.Get();
        info.colors[0].loadOp  = LoadOp::CLEAR;
        info.colors[0].storeOp = StoreOp::STORE;
        enc->BeginRendering(info);
        enc->EndRendering();
    }
    cmdBuf->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmdBuf);
    s.fence = fence.Get();
    queue->Submit(s);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

TEST_F(BarrierTestVulkan, MemoryBarrierBetweenComputeDispatches)
{
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);

    auto pool   = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto fence  = MakeFence(device);
    auto *cmdBuf = pool->Allocate();

    cmdBuf->Begin();
    {
        // Empty cmdbuf with a single memory barrier — exercise the barrier
        // emission path even without an encoder before/after.
        BarrierInfo info{};
        info.srcStage = PipelineStageBit::COMPUTE_SHADER;
        info.dstStage = PipelineStageBit::COMPUTE_SHADER;
        MemoryBarrierInfo mb{};
        mb.srcAccess = AccessFlagBit::COMPUTE_UAV_WRITE;
        mb.dstAccess = AccessFlagBit::COMPUTE_UAV_READ;
        info.memoryBarriers.push_back(mb);
        cmdBuf->PipelineBarrier(info);
    }
    cmdBuf->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmdBuf);
    s.fence = fence.Get();
    queue->Submit(s);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
using BarrierTestMetal = AuroraMetalTest;

TEST_F(BarrierTestMetal, BarrierBeforeRenderEncoderFlushesOnBegin)
{
    // On Metal, PipelineBarrier issued before any encoder must be queued and
    // flushed when the next encoder begins. This test verifies the path.
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);

    auto image  = MakeColorRT(device, 32, 32);
    auto pool   = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto fence  = MakeFence(device);
    auto *cmdBuf = pool->Allocate();

    cmdBuf->Begin();
    {
        BarrierInfo pre = MakeImageTransition(image.Get(),
            AccessFlagBit::NONE, AccessFlagBit::COLOR_WRITE,
            PipelineStageBit::TOP, PipelineStageBit::COLOR_OUTPUT);
        cmdBuf->PipelineBarrier(pre);   // queued (no active encoder yet)

        auto enc = cmdBuf->CreateGraphicsEncoder();
        RenderingInfo info{};
        info.renderArea = {{0, 0}, {32, 32}};
        info.numColors  = 1;
        info.colors[0].image   = image.Get();
        info.colors[0].loadOp  = LoadOp::CLEAR;
        info.colors[0].storeOp = StoreOp::STORE;
        enc->BeginRendering(info);     // expect pending barriers flushed here
        enc->EndRendering();
    }
    cmdBuf->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmdBuf);
    s.fence = fence.Get();
    queue->Submit(s);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}
#endif
