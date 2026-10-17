#include "GpuTestSupport.h"
#include "prepare/ScenePrepareScheduler.h"
#include <VulkanDevice.h>
#include <aurora/rhi/Instance.h>
#include <aurora/rhi/Shader.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <thread>

using namespace sky;
using namespace sky::aurora;
using namespace std::chrono_literals;

namespace {
    class GpuSubmission : public testing::TestWithParam<API> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"RendererGpuProbe", "SkyEngine", true, GetParam()});
            device = Instance::Get()->GetDevice();
            ASSERT_NE(device, nullptr);
            queue = device->GetQueue(QueueType::GRAPHICS);
            ASSERT_NE(queue, nullptr);
        }
        void TearDown() override
        {
            Instance::Destroy();
        }
        Device *device = nullptr;
        Queue  *queue  = nullptr;
    };
} // namespace

TEST_P(GpuSubmission, ShaderCreationRejectsUnownedFunctionsAndNonBinaryProviders)
{
    CounterPtr<ShaderDataProvider> provider(new ShaderDataProvider());
    ShaderFunctionPtr              unsupported(device->CreateShaderFunction({ShaderStageFlagBit::VS, provider, {}}));
    EXPECT_FALSE(unsupported);

    ShaderFunctionPtr  function(new ShaderFunction());
    ShaderReflection   reflection;
    Shader::Descriptor descriptor{};
    descriptor.vs         = function.Get();
    descriptor.ps         = function.Get();
    descriptor.reflection = &reflection;
    ShaderPtr shader(device->CreateShader(descriptor));
    EXPECT_FALSE(shader);
}

TEST_P(GpuSubmission, PreparedOwnedPayloadsCopyInTwoOutstandingSubmissionsAndReuse)
{
    struct Slot {
        std::unique_ptr<CommandPool> pool;
        CommandBuffer               *command = nullptr;
        BufferPtr                    upload;
        BufferPtr                    readback;
        FencePtr                     fence;
    };
    std::array<Slot, 2> slots;
    for (auto &slot : slots) {
        slot.pool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
        ASSERT_TRUE(slot.pool);
        slot.command = slot.pool->Allocate();
        ASSERT_NE(slot.command, nullptr);
        Buffer::Descriptor input;
        input.size   = 64;
        input.usage  = BufferUsageFlagBit::TRANSFER_SRC;
        input.memory = MemoryType::CPU_ONLY;
        slot.upload  = device->CreateBuffer(input);
        auto output  = input;
        output.usage = BufferUsageFlagBit::TRANSFER_DST;
        // Vulkan CPU_ONLY requires HOST_COHERENT; this probe does not exercise noncoherent staging.
        output.memory = GetParam() == API::VULKAN ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU;
        slot.readback = device->CreateBuffer(output);
        slot.fence    = device->CreateFence({true});
        ASSERT_TRUE(slot.upload && slot.readback && slot.fence);
    }
    sky::aurora::Semaphore::Descriptor gateDesc;
    gateDesc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(gateDesc));
    ASSERT_TRUE(gate);
    for (uint32_t round = 1; round <= 3; ++round) {
        auto products = PreparePayload(round * 100);
        ASSERT_TRUE(products);
        ASSERT_EQ(products->records.size(), 2u);
        Finally drain{[&] {
            if (gate->GetCurrentValue() < round)
                gate->Signal(round);
            queue->WaitIdle();
        }};
        for (size_t i = 0; i < slots.size(); ++i) {
            auto &slot = slots[i];
            ASSERT_EQ(slot.fence->Query().status, FenceStatus::SIGNALED);
            auto bytes = products->Read(products->records[i]);
            ASSERT_EQ(bytes.size(), 64u);
            auto *mapped = slot.upload->Map();
            ASSERT_NE(mapped, nullptr);
            std::memcpy(mapped, bytes.data(), bytes.size());
            slot.upload->UnMap();
            slot.pool->Reset();
            slot.command->Begin();
            {
                auto blit = slot.command->CreateBlitEncoder();
                ASSERT_TRUE(blit);
                blit->CopyBuffer(slot.upload.Get(), slot.readback.Get(), 64, 0, 0);
            }
            HostReadBarrier(device, slot.command);
            slot.command->End();
            ASSERT_TRUE(slot.fence->TryReset().Succeeded());
            EXPECT_EQ(slot.fence->Query().status, FenceStatus::PENDING);
            SubmitInfo submit;
            submit.commandBuffers = {slot.command};
            submit.waitSemaphores = {{gate.Get(), round, PipelineStageBit::TRANSFER}};
            submit.fence          = slot.fence.Get();
            ASSERT_EQ(queue->TrySubmit(submit).status, QueueSubmitStatus::ACCEPTED);
        }
        for (auto &slot : slots)
            EXPECT_EQ(slot.fence->Query().status, FenceStatus::PENDING);
        gate->Signal(round);
        for (size_t i = 0; i < slots.size(); ++i) {
            auto &slot = slots[i];
            ASSERT_EQ(Await(*slot.fence).status, FenceStatus::SIGNALED);
            auto *mapped = slot.readback->Map();
            ASSERT_NE(mapped, nullptr);
            EXPECT_EQ(std::memcmp(mapped, products->Read(products->records[i]).data(), 64), 0);
            slot.readback->UnMap();
        }
    }
}

TEST_P(GpuSubmission, RejectionAfterResetDoesNotPretendCompletionAndCanRetry)
{
    FencePtr fence(device->CreateFence({true}));
    ASSERT_TRUE(fence);
    ASSERT_TRUE(fence->TryReset().Succeeded());
    SubmitInfo rejected;
    rejected.commandBuffers = {nullptr};
    rejected.fence          = fence.Get();
    const auto result       = queue->TrySubmit(rejected);
    EXPECT_EQ(result.status, QueueSubmitStatus::REJECTED);
    EXPECT_EQ(result.error.code, GpuErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
    rejected.commandBuffers.clear();
    EXPECT_EQ(queue->TrySubmit(rejected).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(Await(*fence).status, FenceStatus::SIGNALED);
    queue->WaitIdle();
}

TEST_P(GpuSubmission, SemaphorePreflightRunsBeforeAnyQueueWait)
{
    sky::aurora::Semaphore::Descriptor desc;
    desc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(desc));
    ASSERT_TRUE(gate);
    SubmitInfo invalid;
    invalid.waitSemaphores   = {{gate.Get(), 1, PipelineStageBit::TRANSFER}};
    invalid.signalSemaphores = {{nullptr, 0, PipelineStageBit::BOTTOM}};
    FencePtr witness(device->CreateFence({false}));
    ASSERT_TRUE(witness);
    Finally cleanup{[&] {
        gate->Signal(1);
        queue->WaitIdle();
    }};
    EXPECT_EQ(queue->TrySubmit(invalid).status, QueueSubmitStatus::REJECTED);
    SubmitInfo valid;
    valid.fence = witness.Get();
    ASSERT_EQ(queue->TrySubmit(valid).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(Await(*witness).status, FenceStatus::SIGNALED);
}

TEST_P(GpuSubmission, BinarySemaphoreIgnoresSubmittedValue)
{
    sky::aurora::Semaphore::Descriptor desc;
    desc.type = SemaphoreType::BINARY;
    SemaphorePtr semaphore(device->CreateSema(desc));
    FencePtr     fence(device->CreateFence({false}));
    ASSERT_TRUE(semaphore && fence);
    Finally    drain{[&] { queue->WaitIdle(); }};
    SubmitInfo signal;
    signal.signalSemaphores = {{semaphore.Get(), UINT64_MAX, PipelineStageBit::BOTTOM}};
    ASSERT_EQ(queue->TrySubmit(signal).status, QueueSubmitStatus::ACCEPTED);
    SubmitInfo wait;
    wait.waitSemaphores = {{semaphore.Get(), UINT64_MAX, PipelineStageBit::TOP}};
    wait.fence          = fence.Get();
    ASSERT_EQ(queue->TrySubmit(wait).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(Await(*fence).status, FenceStatus::SIGNALED);
}

INSTANTIATE_TEST_SUITE_P(WindowsBackends, GpuSubmission, testing::Values(API::VULKAN, API::DX12), [](const auto &info) {
    return info.param == API::VULKAN ? "Vulkan" : "DX12";
});

namespace {
    VkResult                       injectedResult = VK_SUCCESS;
    uint32_t                       submitCalls    = 0;
    VKAPI_ATTR VkResult VKAPI_CALL InjectSubmit(VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence)
    {
        ++submitCalls;
        return injectedResult;
    }
    VKAPI_ATTR VkResult VKAPI_CALL InjectQuery(VkDevice, VkFence)
    {
        return injectedResult;
    }
    VKAPI_ATTR VkResult VKAPI_CALL InjectReset(VkDevice, uint32_t, const VkFence *)
    {
        return injectedResult;
    }
    class VulkanSubmissionFault : public testing::Test {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"RendererGpuFault", "SkyEngine", true, API::VULKAN});
            device = static_cast<VulkanDevice *>(Instance::Get()->GetDevice());
            ASSERT_NE(device, nullptr);
        }
        void TearDown() override
        {
            Instance::Destroy();
        }
        VulkanDevice *device = nullptr;
    };
} // namespace

TEST_F(VulkanSubmissionFault, NativeSubmitErrorsPreserveDefiniteVersusUncertainOutcome)
{
    auto      &functions = const_cast<VulkanDeviceFunctions &>(device->GetDeviceFn());
    const auto original  = functions.vkQueueSubmit2;
    Finally    restore{[&] { functions.vkQueueSubmit2 = original; }};
    functions.vkQueueSubmit2 = InjectSubmit;
    auto    *queue           = device->GetQueue(QueueType::GRAPHICS);
    FencePtr fence(device->CreateFence({true}));
    ASSERT_TRUE(fence->TryReset().Succeeded());
    SubmitInfo info;
    info.fence = fence.Get();
    for (auto error : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_DEVICE_LOST, VK_ERROR_UNKNOWN}) {
        injectedResult = error;
        submitCalls    = 0;
        auto result    = queue->TrySubmit(info);
        EXPECT_EQ(submitCalls, 1u);
        EXPECT_EQ(result.status, error == VK_ERROR_OUT_OF_HOST_MEMORY || error == VK_ERROR_OUT_OF_DEVICE_MEMORY ? QueueSubmitStatus::REJECTED
                                                                                                                : QueueSubmitStatus::INDETERMINATE);
        EXPECT_EQ(result.error.nativeCode, error);
        EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
    }
    functions.vkQueueSubmit2 = original;
    EXPECT_EQ(queue->TrySubmit(info).status, QueueSubmitStatus::ACCEPTED);
    EXPECT_EQ(Await(*fence).status, FenceStatus::SIGNALED);
}

TEST_F(VulkanSubmissionFault, FenceErrorsNeverBecomePendingOrSuccess)
{
    FencePtr   fence(device->CreateFence({true}));
    auto      &functions = const_cast<VulkanDeviceFunctions &>(device->GetDeviceFn());
    const auto query     = functions.vkGetFenceStatus;
    const auto reset     = functions.vkResetFences;
    Finally    restore{[&] {
        functions.vkGetFenceStatus = query;
        functions.vkResetFences    = reset;
    }};
    functions.vkGetFenceStatus = InjectQuery;
    functions.vkResetFences    = InjectReset;
    injectedResult             = VK_NOT_READY;
    EXPECT_EQ(fence->Query().status, FenceStatus::PENDING);
    for (auto error : {VK_ERROR_DEVICE_LOST, VK_ERROR_UNKNOWN}) {
        injectedResult = error;
        EXPECT_EQ(fence->Query().status, FenceStatus::FAILED);
        EXPECT_FALSE(fence->IsSignaled());
        EXPECT_FALSE(fence->TryReset().Succeeded());
        EXPECT_EQ(fence->TryReset().error.nativeCode, error);
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
