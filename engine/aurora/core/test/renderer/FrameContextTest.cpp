#include "GpuTestSupport.h"
#include <algorithm>
#include <common/FrameContext.h>

namespace {
    class FrameSlots : public testing::TestWithParam<API> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"FrameSlotTest", "SkyEngine", true, GetParam()});
            device = Instance::Get()->GetDevice();
            ASSERT_NE(device, nullptr);
            DeviceFrameContextInitInfo info{2, 1, 27, 2};
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            backend = static_cast<BackendFrameContext *>(context.get());
            ASSERT_NE(backend, nullptr);
        }
        void TearDown() override
        {
            context.reset();
            Instance::Destroy();
        }
        std::unique_ptr<RecordedFrame> RecordEmpty(const DeviceFrameLease &lease, FrameRecordingInputs inputs = {})
        {
            auto allocated = backend->AllocateCommand(lease, 0);
            if (!std::holds_alternative<CommandBuffer *>(allocated)) {
                ADD_FAILURE() << "Command allocation failed";
                return {};
            }
            auto *command = std::get<CommandBuffer *>(allocated);
            command->Begin();
            command->End();
            auto recorded = backend->FinishRecording(lease, std::move(inputs));
            if (!std::holds_alternative<std::unique_ptr<RecordedFrame>>(recorded)) {
                ADD_FAILURE() << "Recording hand-off failed";
                return {};
            }
            return std::move(std::get<std::unique_ptr<RecordedFrame>>(recorded));
        }
        std::vector<FrameCompletion> Collect(size_t expected)
        {
            std::vector<FrameCompletion> result;
            const auto                   deadline = std::chrono::steady_clock::now() + 5s;
            while (result.size() < expected && std::chrono::steady_clock::now() < deadline) {
                std::array<FrameCompletion, 2> batch;
                const auto count = context->PollCompleted(std::span(batch).first(std::min(batch.size(), expected - result.size())));
                for (size_t i = 0; i < count; ++i)
                    result.push_back(std::move(batch[i]));
                if (result.size() < expected)
                    std::this_thread::sleep_for(1ms);
            }
            return result;
        }
        Device                             *device = nullptr;
        std::unique_ptr<DeviceFrameContext> context;
        BackendFrameContext                *backend = nullptr;
    };

    class VulkanFrameSlots : public FrameSlots {};

    VkResult VKAPI_PTR RejectSlotSubmit(VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    VkResult VKAPI_PTR UncertainSlotSubmit(VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence)
    {
        return VK_ERROR_UNKNOWN;
    }
    VkResult VKAPI_PTR FailedSlotQuery(VkDevice, VkFence)
    {
        return VK_ERROR_DEVICE_LOST;
    }
    VkResult VKAPI_PTR FailedSlotReset(VkDevice, uint32_t, const VkFence *)
    {
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    }
    VkResult VKAPI_PTR UnexpectedSlotFenceReset(VkDevice, uint32_t, const VkFence *)
    {
        ADD_FAILURE() << "Acquiring/discarding a lease must not reset a fence";
        return VK_ERROR_UNKNOWN;
    }
} // namespace

TEST_P(FrameSlots, EmptyLeasesValidateIdentityAndDoNotCreateSubmissionWaits)
{
    auto begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    const auto lease = std::get<DeviceFrameLease>(begin);
    EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
    for (int field = 0; field < 4; ++field) {
        auto invalid = lease;
        if (field == 0)
            ++invalid.context.contextId;
        if (field == 1)
            ++invalid.context.deviceEpoch;
        if (field == 2)
            ++invalid.generation;
        if (field == 3)
            invalid.slot = UINT32_MAX;
        EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(invalid)));
        EXPECT_TRUE(std::holds_alternative<FrameError>(backend->AllocateCommand(invalid, 0)));
    }
    EXPECT_EQ(context->GetFrameFence(), nullptr);
    context->BeginFrame();
    context->EndFrame();
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    for (int i = 0; i < 32; ++i) {
        begin = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
        auto next = std::get<DeviceFrameLease>(begin);
        if (next.slot == lease.slot)
            EXPECT_GT(next.generation, lease.generation);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
    }
    EXPECT_TRUE(Collect(0).empty());
    std::unique_ptr<DeviceFrameContext> direct(device->CreateFrameContext({}));
    ASSERT_TRUE(direct);
    ASSERT_NE(direct->GetFrameFence(), nullptr);
    EXPECT_TRUE(std::holds_alternative<FrameError>(direct->TryBeginSlot()));
}

TEST_P(FrameSlots, TwoSlotsKeepCommandsAndOwnedPrepareProductsUntilCompletion)
{
    struct Buffers {
        BufferPtr upload;
        BufferPtr readback;
    };
    std::array<Buffers, 2> buffers;
    for (auto &pair : buffers) {
        Buffer::Descriptor desc;
        desc.size     = 64;
        desc.memory   = MemoryType::CPU_ONLY;
        desc.usage    = BufferUsageFlagBit::TRANSFER_SRC;
        pair.upload   = device->CreateBuffer(desc);
        desc.memory   = GetParam() == API::VULKAN ? MemoryType::CPU_ONLY : MemoryType::GPU_TO_CPU;
        desc.usage    = BufferUsageFlagBit::TRANSFER_DST;
        pair.readback = device->CreateBuffer(desc);
        ASSERT_TRUE(pair.upload && pair.readback);
    }
    sky::aurora::Semaphore::Descriptor gateDesc;
    gateDesc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(gateDesc));
    ASSERT_TRUE(gate);
    for (uint32_t round = 1; round <= 3; ++round) {
        Finally drain{[&] {
            if (gate->GetCurrentValue() < round)
                gate->Signal(round);
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
        }};
        auto    products = PreparePayload(round * 11);
        ASSERT_TRUE(products);
        ASSERT_EQ(products->records.size(), 2u);
        std::weak_ptr<const PrepareProducts>     weak = products;
        std::array<std::array<std::byte, 64>, 2> expected;
        std::array<SubmissionToken, 2>           submitted;
        std::array<CommandBuffer *, 4>           commands;
        for (uint32_t slot = 0; slot < 2; ++slot) {
            auto begin = context->TryBeginSlot();
            ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
            const auto lease = std::get<DeviceFrameLease>(begin);
            auto       bytes = products->Read(products->records[slot]);
            ASSERT_EQ(bytes.size(), 64u);
            std::memcpy(expected[slot].data(), bytes.data(), 64);
            auto *mapped = buffers[slot].upload->Map();
            ASSERT_NE(mapped, nullptr);
            std::memcpy(mapped, bytes.data(), 64);
            buffers[slot].upload->UnMap();
            for (uint32_t ordinal = 0; ordinal < 2; ++ordinal) {
                auto allocated = backend->AllocateCommand(lease, ordinal);
                ASSERT_TRUE(std::holds_alternative<CommandBuffer *>(allocated));
                auto *command                = std::get<CommandBuffer *>(allocated);
                commands[slot * 2 + ordinal] = command;
                command->Begin();
                {
                    auto blit = command->CreateBlitEncoder();
                    ASSERT_TRUE(blit);
                    blit->CopyBuffer(buffers[slot].upload.Get(), buffers[slot].readback.Get(), 32, ordinal * 32, ordinal * 32);
                }
                HostReadBarrier(device, command);
                command->End();
            }
            EXPECT_TRUE(std::holds_alternative<FrameError>(backend->AllocateCommand(lease, 0)));
            EXPECT_TRUE(std::holds_alternative<FrameError>(backend->AllocateCommand(lease, 2)));
            FrameRecordingInputs inputs;
            inputs.waits     = {{gate.Get(), round, PipelineStageBit::TRANSFER}};
            inputs.resources = {buffers[slot].upload, buffers[slot].readback};
            inputs.payload   = products;
            auto finished    = backend->FinishRecording(lease, std::move(inputs));
            ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(finished));
            auto recorded = std::move(std::get<std::unique_ptr<RecordedFrame>>(finished));
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
            const auto result = context->Submit(recorded);
            ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(result));
            EXPECT_FALSE(recorded);
            submitted[slot] = std::get<SubmissionAccepted>(result).token;
            EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
        }
        for (size_t i = 0; i < commands.size(); ++i)
            for (size_t j = i + 1; j < commands.size(); ++j)
                EXPECT_NE(commands[i], commands[j]);
        products.reset();
        EXPECT_FALSE(weak.expired());
        EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
        std::array<FrameCompletion, 2> pending;
        EXPECT_EQ(context->PollCompleted(pending), 0u);
        EXPECT_EQ(context->PollCompleted({}), 0u);
        gate->Signal(round);
        device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
        EXPECT_EQ(context->PollCompleted({}), 0u);
        EXPECT_FALSE(weak.expired());
        EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
        auto completions = Collect(1);
        ASSERT_EQ(completions.size(), 1u);
        EXPECT_FALSE(weak.expired());
        auto remaining = Collect(1);
        completions.insert(completions.end(), remaining.begin(), remaining.end());
        ASSERT_EQ(completions.size(), 2u);
        for (const auto &completion : completions) {
            EXPECT_EQ(completion.status, FrameCompletionStatus::COMPLETE);
            EXPECT_TRUE(completion.token == submitted[0] || completion.token == submitted[1]);
        }
        EXPECT_NE(completions[0].token, completions[1].token);
        EXPECT_EQ(context->PollCompleted(pending), 0u);
        EXPECT_TRUE(weak.expired());
        for (size_t slot = 0; slot < buffers.size(); ++slot) {
            auto *mapped = buffers[slot].readback->Map();
            ASSERT_NE(mapped, nullptr);
            EXPECT_EQ(std::memcmp(mapped, expected[slot].data(), 64), 0);
            buffers[slot].readback->UnMap();
        }
    }
}

TEST_P(FrameSlots, RejectedRecordingPreservesOwnershipAndDiscardAllowsReuse)
{
    auto begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    auto                 lease    = std::get<DeviceFrameLease>(begin);
    auto                 lifetime = std::make_shared<int>(17);
    std::weak_ptr<int>   weak     = lifetime;
    FrameRecordingInputs inputs;
    inputs.signals = {{nullptr, 0, PipelineStageBit::BOTTOM}};
    inputs.payload = lifetime;
    auto recorded  = RecordEmpty(lease, std::move(inputs));
    ASSERT_TRUE(recorded);
    lifetime.reset();
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext({2, 1, 27}));
    ASSERT_TRUE(other);
    auto wrong = other->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(wrong));
    EXPECT_EQ(std::get<SubmissionRejected>(wrong).error.code, FrameErrorCode::WRONG_CONTEXT);
    auto result = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(result));
    EXPECT_EQ(std::get<SubmissionRejected>(result).error.cause.code, GpuErrorCode::INVALID_ARGUMENT);
    ASSERT_TRUE(recorded);
    EXPECT_FALSE(weak.expired());
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    recorded.reset();
    EXPECT_TRUE(weak.expired());
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
    for (int i = 0; i < 3; ++i) {
        begin = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
        lease    = std::get<DeviceFrameLease>(begin);
        recorded = RecordEmpty(lease);
        ASSERT_TRUE(recorded);
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recorded)));
        auto completed = Collect(1);
        ASSERT_EQ(completed.size(), 1u);
        EXPECT_EQ(completed[0].status, FrameCompletionStatus::COMPLETE);
    }
}

TEST_P(FrameSlots, InvalidCreationAndNullGraphCallsAreExplicit)
{
    EXPECT_EQ(device->CreateFrameContext({0, 1, 1}), nullptr);
    EXPECT_EQ(device->CreateFrameContext({2, 0, 1}), nullptr);
    EXPECT_EQ(device->CreateFrameContext({2, 1, 0}), nullptr);
    EXPECT_EQ(device->CreateFrameContext({2, 1, 1, 0}), nullptr);
    auto begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    const auto lease   = std::get<DeviceFrameLease>(begin);
    const auto compile = context->Compile(lease, {});
    ASSERT_TRUE(std::holds_alternative<FrameError>(compile));
    EXPECT_EQ(std::get<FrameError>(compile).code, FrameErrorCode::INVALID_ARGUMENT);
    ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(VulkanFrameSlots, VulkanResetAndRejectedSubmitCanRetrySameRecording)
{
    auto   &fn     = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
    auto    reset  = fn.vkResetFences;
    auto    submit = fn.vkQueueSubmit2;
    Finally restore{[&] {
        fn.vkResetFences  = reset;
        fn.vkQueueSubmit2 = submit;
    }};
    fn.vkResetFences = UnexpectedSlotFenceReset;
    for (int i = 0; i < 4; ++i) {
        auto begin = context->TryBeginSlot();
        ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
        ASSERT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(std::get<DeviceFrameLease>(begin))));
    }
    auto begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    auto recorded = RecordEmpty(std::get<DeviceFrameLease>(begin));
    ASSERT_TRUE(recorded);
    fn.vkResetFences = FailedSlotReset;
    auto result      = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(result));
    EXPECT_EQ(std::get<SubmissionRejected>(result).error.cause.nativeCode, VK_ERROR_OUT_OF_HOST_MEMORY);
    EXPECT_TRUE(recorded);
    fn.vkResetFences  = reset;
    fn.vkQueueSubmit2 = RejectSlotSubmit;
    result            = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionRejected>(result));
    EXPECT_EQ(std::get<SubmissionRejected>(result).error.cause.code, GpuErrorCode::OUT_OF_MEMORY);
    EXPECT_TRUE(recorded);
    std::array<FrameCompletion, 2> completions;
    EXPECT_EQ(context->PollCompleted(completions), 0u);
    fn.vkQueueSubmit2 = submit;
    result            = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(result));
    EXPECT_FALSE(recorded);
    EXPECT_EQ(std::get<SubmissionAccepted>(result).token.serial, 1u);
    EXPECT_EQ(Collect(1).size(), 1u);
}

TEST_P(VulkanFrameSlots, VulkanIndeterminateAndFailedQueryQuarantineUntilTeardown)
{
    auto   &fn     = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
    auto    submit = fn.vkQueueSubmit2;
    auto    query  = fn.vkGetFenceStatus;
    Finally restore{[&] {
        fn.vkQueueSubmit2   = submit;
        fn.vkGetFenceStatus = query;
    }};
    auto    begin = context->TryBeginSlot();
    ASSERT_TRUE(std::holds_alternative<DeviceFrameLease>(begin));
    const auto           lease    = std::get<DeviceFrameLease>(begin);
    auto                 lifetime = std::make_shared<int>(23);
    std::weak_ptr<int>   weak     = lifetime;
    FrameRecordingInputs inputs;
    inputs.payload = lifetime;
    auto recorded  = RecordEmpty(lease, std::move(inputs));
    lifetime.reset();
    fn.vkQueueSubmit2 = UncertainSlotSubmit;
    auto result       = context->Submit(recorded);
    ASSERT_TRUE(std::holds_alternative<SubmissionIndeterminate>(result));
    EXPECT_FALSE(recorded);
    const auto token = std::get<SubmissionIndeterminate>(result).token;
    EXPECT_EQ(std::get<SubmissionIndeterminate>(result).error.cause.nativeCode, VK_ERROR_UNKNOWN);
    fn.vkQueueSubmit2 = submit;
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->DiscardUnsubmitted(lease)));
    std::array<FrameCompletion, 1> completions;
    EXPECT_EQ(context->PollCompleted(completions), 0u);
    EXPECT_FALSE(weak.expired());
    fn.vkGetFenceStatus = FailedSlotQuery;
    ASSERT_EQ(context->PollCompleted(completions), 1u);
    EXPECT_EQ(completions[0].token, token);
    EXPECT_EQ(completions[0].status, FrameCompletionStatus::DEVICE_LOST);
    EXPECT_EQ(context->PollCompleted(completions), 0u);
    EXPECT_FALSE(weak.expired());
    fn.vkGetFenceStatus = query;
    context.reset();
    EXPECT_TRUE(weak.expired());
}

INSTANTIATE_TEST_SUITE_P(WindowsBackends, FrameSlots, testing::Values(API::VULKAN, API::DX12));

INSTANTIATE_TEST_SUITE_P(VulkanNative, VulkanFrameSlots, testing::Values(API::VULKAN));
