//
// ClientViewport and direct frame context smoke tests.
//

#include "AuroraTestHelper.h"

#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <aurora/rdg/DirectFrameContext.h>
#include <core/name/Name.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

TEST_F(AuroraVulkanTest, DirectFrameContextFenceRing)
{
    auto *device = GetDevice();

    DeviceFrameContextInitInfo info{};
    info.inflightNum = 2;
    info.parallelNum = 1;
    std::unique_ptr<DirectFrameContext> ctx(device->CreateDirectFrameContext(info));
    ASSERT_NE(ctx, nullptr);

    Fence *f0 = ctx->GetFrameFence();
    ASSERT_NE(f0, nullptr);

    EXPECT_EQ(ctx->GetFrameFence(), f0);
    EXPECT_TRUE(f0->IsSignaled());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(ctx->TryBeginFrame()));
    SubmitInfo submit;
    submit.fence = f0;
    ASSERT_EQ(device->GetQueue(QueueType::GRAPHICS)->TrySubmit(submit).status, QueueSubmitStatus::ACCEPTED);
    ASSERT_TRUE(f0->WaitFor(5'000'000'000ULL));
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(ctx->EndFrame()));

    Fence *f1 = ctx->GetFrameFence();
    ASSERT_NE(f1, nullptr);
    EXPECT_NE(f0, f1); // different slot for the next frame
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
