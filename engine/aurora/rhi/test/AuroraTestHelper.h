//
// Created on 2026/04/01.
//

#pragma once

#include <gtest/gtest.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/Device.h>

namespace sky::aurora::test {

    class AuroraVulkanTest : public ::testing::Test {
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

        Device *GetDevice() const { return Instance::Get()->GetDevice(); }
    };

    class AuroraD3D12Test : public ::testing::Test {
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

        Device *GetDevice() const { return Instance::Get()->GetDevice(); }
    };

    class AuroraGLESTest : public ::testing::Test {
    public:
        static void SetUpTestSuite()
        {
            Instance::Descriptor desc = {};
            desc.appName          = "AuroraTest";
            desc.engineName       = "SkyEngine";
            desc.enableDebugLayer = false;
            desc.api              = API::GLES;
            Instance::Get()->Init(desc);
        }

        static void TearDownTestSuite()
        {
            Instance::Destroy();
        }

        Device *GetDevice() const { return Instance::Get()->GetDevice(); }
    };

} // namespace sky::aurora::test
