//
// ClientViewport and direct frame context smoke tests.
//

#include "AuroraTestHelper.h"

#include <aurora/rdg/ClientViewport.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <core/name/Name.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

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

TEST_F(AuroraVulkanTest, ClientViewportUninitializedSafe)
{
    ClientViewport viewport(Name("uninit"));

    // Without Init() the viewport must be safe: no crash, no present.
    EXPECT_FALSE(viewport.Begin());
    EXPECT_FALSE(viewport.Acquire());
    viewport.Release(); // no-op
    EXPECT_EQ(viewport.GetBackbuffer(), nullptr);
}
