//
// Created by Zach Lee on 2026/3/30.
//

#include "AuroraTestHelper.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

using DeviceTestVulkan = AuroraVulkanTest;
using DeviceTestD3D12  = AuroraD3D12Test;
using DeviceTestGLES   = AuroraGLESTest;

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