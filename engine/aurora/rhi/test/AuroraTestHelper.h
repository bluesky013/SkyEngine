//
// Created on 2026/04/01.
//

#pragma once

#include <gtest/gtest.h>
#include <core/platform/Platform.h>
#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/Device.h>
#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>

namespace sky::aurora::test {

    inline CounterPtr<Fence> MakeFence(Device *device, bool createSignaled = false)
    {
        Fence::Descriptor desc{};
        desc.createSignaled = createSignaled;
        return CounterPtr<Fence>(device->CreateFence(desc));
    }

    inline CounterPtr<Semaphore> MakeBinarySema(Device *device)
    {
        Semaphore::Descriptor desc{};
        desc.type = SemaphoreType::BINARY;
        return CounterPtr<Semaphore>(device->CreateSema(desc));
    }

    inline CounterPtr<Semaphore> MakeTimelineSema(Device *device, uint64_t initial = 0)
    {
        Semaphore::Descriptor desc{};
        desc.type         = SemaphoreType::TIMELINE;
        desc.initialValue = initial;
        return CounterPtr<Semaphore>(device->CreateSema(desc));
    }


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

#if defined(SKY_PLATFORM_WINDOWS)
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
#endif

#if defined(SKY_PLATFORM_MACOS) || defined(SKY_PLATFORM_IOS)
    class AuroraMetalTest : public ::testing::Test {
    public:
        static void SetUpTestSuite()
        {
            Instance::Descriptor desc = {};
            desc.appName          = "AuroraTest";
            desc.engineName       = "SkyEngine";
            desc.enableDebugLayer = true;
            desc.api              = API::METAL;
            Instance::Get()->Init(desc);
        }

        static void TearDownTestSuite()
        {
            Instance::Destroy();
        }

        Device *GetDevice() const { return Instance::Get()->GetDevice(); }
    };
#endif

#if defined(SKY_AURORA_HAS_GLES)
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
#endif

} // namespace sky::aurora::test
