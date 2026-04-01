//
// Created by Zach Lee on 2026/3/30.
//

#include "AuroraTestHelper.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

using DeviceTestVulkan = AuroraVulkanTest;
#if defined(SKY_PLATFORM_WINDOWS)
using DeviceTestD3D12  = AuroraD3D12Test;
#endif
#if defined(SKY_AURORA_HAS_GLES)
using DeviceTestGLES   = AuroraGLESTest;
#endif
#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
using DeviceTestMetal  = AuroraMetalTest;
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
#endif

#if defined(SKY_AURORA_HAS_GLES)
TEST_F(DeviceTestGLES, InstanceInitialized)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceTestGLES, DeviceInfo)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);
    const auto &info = device->GetDeviceInfo();
    EXPECT_FALSE(info.empty());
}
#endif
