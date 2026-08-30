//
// Aurora RHI Submit / Queue tests (headless).
//

#include "AuroraTestHelper.h"

#include <aurora/rhi/CommandBuffer.h>
#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/Queue.h>
#include <aurora/rhi/SubmitInfo.h>

#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

using SubmitTestVulkan = AuroraVulkanTest;

TEST_F(SubmitTestVulkan, GetGraphicsQueue)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *q = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->GetType(), QueueType::GRAPHICS);

    EXPECT_EQ(device->GetQueue(QueueType::GRAPHICS), q);   // stable identity
}

TEST_F(SubmitTestVulkan, SubmitEmptyCmdBufWithFence)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(queue, nullptr);

    auto fence = MakeFence(device, /*createSignaled=*/false);
    ASSERT_NE(fence.Get(), nullptr);
    EXPECT_FALSE(fence->IsSignaled());

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    ASSERT_NE(pool.get(), nullptr);

    auto *cmdBuf = pool->Allocate();
    ASSERT_NE(cmdBuf, nullptr);

    cmdBuf->Begin();
    cmdBuf->End();

    SubmitInfo submit{};
    submit.commandBuffers.push_back(cmdBuf);
    submit.fence = fence.Get();

    queue->Submit(submit);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));        // 5 seconds
    EXPECT_TRUE(fence->IsSignaled());
}

TEST_F(SubmitTestVulkan, FenceWaitForZeroReturnsFalseBeforeCompletion)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(queue, nullptr);

    auto fence = MakeFence(device);
    ASSERT_NE(fence.Get(), nullptr);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmdBuf = pool->Allocate();
    cmdBuf->Begin();
    cmdBuf->End();

    SubmitInfo submit{};
    submit.commandBuffers.push_back(cmdBuf);
    submit.fence = fence.Get();
    queue->Submit(submit);

    // WaitFor(0) is a non-blocking poll; it MAY succeed if the GPU completed
    // an empty cmdbuf instantly, or fail if not yet. Either way it must not block.
    (void)fence->WaitFor(0);

    fence->Wait();
    EXPECT_TRUE(fence->IsSignaled());
}

TEST_F(SubmitTestVulkan, BinarySemaphoreChainBetweenSubmits)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(queue, nullptr);

    auto sema = MakeBinarySema(device);
    ASSERT_NE(sema.Get(), nullptr);
    EXPECT_EQ(sema->GetType(), SemaphoreType::BINARY);

    auto fence = MakeFence(device);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd1 = pool->Allocate();
    cmd1->Begin();
    cmd1->End();
    auto *cmd2 = pool->Allocate();
    cmd2->Begin();
    cmd2->End();

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd1);
        SemaphoreSubmitInfo sig{};
        sig.semaphore = sema.Get();
        sig.stageMask = PipelineStageBit::BOTTOM;
        s.signalSemaphores.push_back(sig);
        queue->Submit(s);
    }

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd2);
        SemaphoreSubmitInfo wait{};
        wait.semaphore = sema.Get();
        wait.stageMask = PipelineStageBit::TOP;
        s.waitSemaphores.push_back(wait);
        s.fence = fence.Get();
        queue->Submit(s);
    }

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

TEST_F(SubmitTestVulkan, TimelineSemaphoreCrossSubmit)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    auto timeline = MakeTimelineSema(device, 0);
    ASSERT_NE(timeline.Get(), nullptr);
    EXPECT_EQ(timeline->GetType(), SemaphoreType::TIMELINE);
    EXPECT_EQ(timeline->GetCurrentValue(), 0u);

    auto fence = MakeFence(device);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd1 = pool->Allocate();
    cmd1->Begin(); cmd1->End();
    auto *cmd2 = pool->Allocate();
    cmd2->Begin(); cmd2->End();

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd1);
        SemaphoreSubmitInfo sig{};
        sig.semaphore = timeline.Get();
        sig.value     = 7;
        sig.stageMask = PipelineStageBit::BOTTOM;
        s.signalSemaphores.push_back(sig);
        queue->Submit(s);
    }

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd2);
        SemaphoreSubmitInfo wait{};
        wait.semaphore = timeline.Get();
        wait.value     = 7;
        wait.stageMask = PipelineStageBit::TOP;
        s.waitSemaphores.push_back(wait);
        s.fence = fence.Get();
        queue->Submit(s);
    }

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
    EXPECT_GE(timeline->GetCurrentValue(), 7u);
}

TEST_F(SubmitTestVulkan, TimelineHostSignalAndWait)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto timeline = MakeTimelineSema(device, 0);
    ASSERT_NE(timeline.Get(), nullptr);

    timeline->Signal(10);
    EXPECT_GE(timeline->GetCurrentValue(), 10u);

    EXPECT_TRUE(timeline->Wait(10, 1'000'000'000ULL));      // immediate
    EXPECT_FALSE(timeline->Wait(100, 1'000'000ULL));         // 1ms timeout, never reached
}

TEST_F(SubmitTestVulkan, MultiThreadRecordSingleSubmit)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    auto *queue = device->GetQueue(QueueType::GRAPHICS);
    auto fence = MakeFence(device);

    constexpr uint32_t NUM_THREADS = 4;
    std::vector<std::unique_ptr<CommandPool>> pools(NUM_THREADS);
    std::vector<CommandBuffer*>          cmdBufs(NUM_THREADS);

    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < NUM_THREADS; ++i) {
        workers.emplace_back([&, i]() {
            pools[i]   = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
            cmdBufs[i] = pools[i]->Allocate();
            cmdBufs[i]->Begin();
            cmdBufs[i]->End();
        });
    }
    for (auto &t : workers) { t.join(); }

    SubmitInfo s{};
    for (auto *cb : cmdBufs) { s.commandBuffers.push_back(cb); }
    s.fence = fence.Get();
    queue->Submit(s);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

TEST_F(SubmitTestVulkan, QueueWaitIdle)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto *queue = device->GetQueue(QueueType::GRAPHICS);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd = pool->Allocate();
    cmd->Begin();
    cmd->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmd);
    queue->Submit(s);

    queue->WaitIdle();      // no fence, but WaitIdle should succeed
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Metal
// ---------------------------------------------------------------------------
#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
using SubmitTestMetal = AuroraMetalTest;

TEST_F(SubmitTestMetal, GetGraphicsQueue)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    auto *q = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(q->GetType(), QueueType::GRAPHICS);
    EXPECT_EQ(device->GetQueue(QueueType::GRAPHICS), q);
}

TEST_F(SubmitTestMetal, SubmitEmptyCmdBufWithFence)
{
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);
    ASSERT_NE(queue, nullptr);

    auto fence = MakeFence(device, false);
    ASSERT_NE(fence.Get(), nullptr);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd = pool->Allocate();
    cmd->Begin();
    cmd->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmd);
    s.fence = fence.Get();
    queue->Submit(s);

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
    EXPECT_TRUE(fence->IsSignaled());
}

TEST_F(SubmitTestMetal, BinarySemaphoreChainBetweenSubmits)
{
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);
    auto sema  = MakeBinarySema(device);
    auto fence = MakeFence(device);
    ASSERT_NE(sema.Get(), nullptr);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd1 = pool->Allocate(); cmd1->Begin(); cmd1->End();
    auto *cmd2 = pool->Allocate(); cmd2->Begin(); cmd2->End();

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd1);
        SemaphoreSubmitInfo sig{}; sig.semaphore = sema.Get();
        s.signalSemaphores.push_back(sig);
        queue->Submit(s);
    }
    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd2);
        SemaphoreSubmitInfo wait{}; wait.semaphore = sema.Get();
        s.waitSemaphores.push_back(wait);
        s.fence = fence.Get();
        queue->Submit(s);
    }

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
}

TEST_F(SubmitTestMetal, TimelineSemaphoreCrossSubmit)
{
    auto *device   = GetDevice();
    auto *queue    = device->GetQueue(QueueType::GRAPHICS);
    auto  timeline = MakeTimelineSema(device, 0);
    auto  fence    = MakeFence(device);
    ASSERT_NE(timeline.Get(), nullptr);
    EXPECT_EQ(timeline->GetType(), SemaphoreType::TIMELINE);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd1 = pool->Allocate(); cmd1->Begin(); cmd1->End();
    auto *cmd2 = pool->Allocate(); cmd2->Begin(); cmd2->End();

    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd1);
        SemaphoreSubmitInfo sig{}; sig.semaphore = timeline.Get(); sig.value = 7;
        s.signalSemaphores.push_back(sig);
        queue->Submit(s);
    }
    {
        SubmitInfo s{};
        s.commandBuffers.push_back(cmd2);
        SemaphoreSubmitInfo wait{}; wait.semaphore = timeline.Get(); wait.value = 7;
        s.waitSemaphores.push_back(wait);
        s.fence = fence.Get();
        queue->Submit(s);
    }

    EXPECT_TRUE(fence->WaitFor(5'000'000'000ULL));
    EXPECT_GE(timeline->GetCurrentValue(), 7u);
}

// SubmitTestMetal.MultiThreadRecordSingleSubmit is intentionally NOT defined:
// raw std::thread workers on Metal lack an NSAutoreleasePool, which causes
// MTLCommandQueue allocations to hang. Use Device::GetParallelContext()
// (which sets up MetalThreadContext per worker) once that path is wired into
// the test helpers.

TEST_F(SubmitTestMetal, QueueWaitIdle)
{
    auto *device = GetDevice();
    auto *queue  = device->GetQueue(QueueType::GRAPHICS);

    auto pool = std::unique_ptr<CommandPool>(device->CreateCommandPool(QueueType::GRAPHICS));
    auto *cmd = pool->Allocate();
    cmd->Begin(); cmd->End();

    SubmitInfo s{};
    s.commandBuffers.push_back(cmd);
    queue->Submit(s);

    queue->WaitIdle();
    SUCCEED();
}
#endif
