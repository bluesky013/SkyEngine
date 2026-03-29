//
// Created by Zach Lee on 2026/3/30.
//

#include <gtest/gtest.h>
#include <aurora/rhi/Instance.h>

using namespace sky::aurora;

class DeviceTestVulkan : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        Instance::Descriptor desc = {};
        desc.appName          = "AuroraTest";
        desc.engineName       = "SkyEngine";
        desc.enableDebugLayer = true;
        desc.api              = API::VULKAN;

        Instance::Get()->Init(desc);
    }

    static void TearDownTestSuite()
    {
        Instance::Destroy();
    }
};

class DeviceTestD3D12 : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        Instance::Descriptor desc = {};
        desc.appName          = "AuroraTest";
        desc.engineName       = "SkyEngine";
        desc.enableDebugLayer = true;
        desc.api              = API::DX12;

        Instance::Get()->Init(desc);
    }

    static void TearDownTestSuite()
    {
        Instance::Destroy();
    }
};

TEST_F(DeviceTestVulkan, InstanceInitialized)
{
    auto *device = Instance::Get()->GetDevice();
    ASSERT_NE(device, nullptr);
}

TEST_F(DeviceTestD3D12, InstanceInitialized)
{
    auto *device = Instance::Get()->GetDevice();
    ASSERT_NE(device, nullptr);
}