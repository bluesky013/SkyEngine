//
// Created by Zach Lee on 2026/3/30.
//

#include "AuroraTestHelper.h"
#include <aurora/rdg/DeviceFrameContext.h>

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

using DeviceTestVulkan = AuroraVulkanTest;
#if defined(SKY_PLATFORM_WINDOWS)
using DeviceTestD3D12 = AuroraD3D12Test;
#endif
#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
using DeviceTestMetal = AuroraMetalTest;
#endif

TEST_F(DeviceTestVulkan, InstanceInitialized)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceTestVulkan, DeviceInfo)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    const auto &info = device->GetDeviceInfo();
    EXPECT_FALSE(info.empty());
}

TEST_F(DeviceTestVulkan, CreateFrameContext)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    DeviceFrameContextInitInfo info{};
    info.inflightNum        = 3;
    info.parallelNum        = 2;
    DeviceFrameContext *ctx = device->CreateFrameContext(info);
    ASSERT_NE(ctx, nullptr);
    delete ctx;
}

#if defined(SKY_PLATFORM_WINDOWS)
TEST_F(DeviceTestD3D12, InstanceInitialized)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceTestD3D12, DeviceInfo)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    const auto &info = device->GetDeviceInfo();
    EXPECT_FALSE(info.empty());
}

TEST_F(DeviceTestD3D12, CreateFrameContext)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    DeviceFrameContextInitInfo info{};
    info.inflightNum        = 3;
    info.parallelNum        = 2;
    DeviceFrameContext *ctx = device->CreateFrameContext(info);
    ASSERT_NE(ctx, nullptr);

    // Legacy BeginFrame resets the fence, so each rotation requires a matching submission.
    for (int i = 0; i < 6; ++i) {
        ctx->BeginFrame();
        SubmitInfo submit;
        submit.fence = ctx->GetFrameFence();
        ASSERT_EQ(device->GetQueue(QueueType::GRAPHICS)->TrySubmit(submit).status, QueueSubmitStatus::ACCEPTED);
        ASSERT_TRUE(submit.fence->WaitFor(5'000'000'000ULL));
        ctx->EndFrame();
    }

    delete ctx;
}
#endif

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
TEST_F(DeviceTestMetal, InstanceInitialized)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceTestMetal, DeviceInfo)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    const auto &info = device->GetDeviceInfo();
    EXPECT_FALSE(info.empty());
}

TEST_F(DeviceTestMetal, CreateFrameContext)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    DeviceFrameContextInitInfo info{};
    info.inflightNum        = 3;
    info.parallelNum        = 2;
    DeviceFrameContext *ctx = device->CreateFrameContext(info);
    ASSERT_NE(ctx, nullptr);
    delete ctx;
}
#endif
