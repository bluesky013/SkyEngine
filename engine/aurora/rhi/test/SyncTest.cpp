//
// Created on 2026/04/01.
//

#include "AuroraTestHelper.h"

using namespace sky;
using namespace sky::aurora;
using namespace sky::aurora::test;

// ---------------------------------------------------------------------------
// Vulkan
// ---------------------------------------------------------------------------
using SyncTestVulkan = AuroraVulkanTest;

TEST_F(SyncTestVulkan, CreateFenceSignaled)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Fence::Descriptor desc = {};
    desc.createSignaled = true;

    auto *fence = device->CreateFence(desc);
    ASSERT_NE(fence, nullptr);
    CounterPtr<Fence> guard(fence);

    // signaled fence should return immediately
    fence->Wait();
    fence->Reset();
}

TEST_F(SyncTestVulkan, CreateFenceUnsignaled)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Fence::Descriptor desc = {};
    desc.createSignaled = false;

    auto *fence = device->CreateFence(desc);
    ASSERT_NE(fence, nullptr);
    CounterPtr<Fence> guard(fence);
}

TEST_F(SyncTestVulkan, CreateSemaphore)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    aurora::Semaphore::Descriptor desc = {};
    desc.initialValue = 0;

    auto *sema = device->CreateSema(desc);
    ASSERT_NE(sema, nullptr);
    CounterPtr<aurora::Semaphore> guard(sema);
}

// ---------------------------------------------------------------------------
// D3D12
// ---------------------------------------------------------------------------
using SyncTestD3D12 = AuroraD3D12Test;

TEST_F(SyncTestD3D12, CreateFenceSignaled)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Fence::Descriptor desc = {};
    desc.createSignaled = true;

    auto *fence = device->CreateFence(desc);
    ASSERT_NE(fence, nullptr);
    CounterPtr<Fence> guard(fence);

    fence->Wait();
    fence->Reset();
}

TEST_F(SyncTestD3D12, CreateSemaphore)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    aurora::Semaphore::Descriptor desc = {};
    desc.initialValue = 0;

    auto *sema = device->CreateSema(desc);
    ASSERT_NE(sema, nullptr);
    CounterPtr<aurora::Semaphore> guard(sema);
}

// ---------------------------------------------------------------------------
// GLES
// ---------------------------------------------------------------------------
using SyncTestGLES = AuroraGLESTest;

TEST_F(SyncTestGLES, CreateFenceSignaled)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    Fence::Descriptor desc = {};
    desc.createSignaled = true;

    auto *fence = device->CreateFence(desc);
    ASSERT_NE(fence, nullptr);
    CounterPtr<Fence> guard(fence);

    fence->Wait();
    fence->Reset();
}

TEST_F(SyncTestGLES, CreateSemaphore)
{
    auto *device = GetDevice();
    ASSERT_NE(device, nullptr);

    aurora::Semaphore::Descriptor desc = {};
    desc.initialValue = 0;

    auto *sema = device->CreateSema(desc);
    ASSERT_NE(sema, nullptr);
    CounterPtr<aurora::Semaphore> guard(sema);
}
