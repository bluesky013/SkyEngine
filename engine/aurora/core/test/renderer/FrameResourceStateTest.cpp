//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"

#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <aurora/rdg/DeviceFrameContext.h>
#include <common/FrameContext.h>
#include <d3d12sdklayers.h>

namespace {
    struct LedgerFaults {
        explicit LedgerFaults(Device &device)
        {
            if (device.GetAPI() == API::VULKAN) {
                vk      = &const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice &>(device).GetDeviceFn());
                savedVk = *vk;
            } else {
                dx      = &const_cast<D3D12Functions &>(static_cast<D3D12Device &>(device).GetFunctions());
                savedDx = *dx;
            }
        }
        ~LedgerFaults()
        {
            Restore();
        }
        void Restore()
        {
            if (vk)
                *vk = savedVk;
            if (dx)
                *dx = savedDx;
        }
        void EncodeFailure()
        {
            if (vk)
                vk->vkEndCommandBuffer = [](VkCommandBuffer) -> VkResult { return VK_ERROR_OUT_OF_HOST_MEMORY; };
            if (dx)
                dx->closeList = [](ID3D12GraphicsCommandList *) -> HRESULT { return E_OUTOFMEMORY; };
        }
        void UncertainSubmit()
        {
            if (vk)
                vk->vkQueueSubmit2 = [](VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence) -> VkResult { return VK_ERROR_UNKNOWN; };
            if (dx)
                dx->signal = [](ID3D12CommandQueue *, ID3D12Fence *, UINT64) -> HRESULT { return E_OUTOFMEMORY; };
        }
        void QueryFailure()
        {
            if (vk)
                vk->vkGetFenceStatus = [](VkDevice, VkFence) -> VkResult { return VK_ERROR_DEVICE_LOST; };
            if (dx)
                dx->completed = [](ID3D12Fence *) -> UINT64 { return UINT64_MAX; };
        }
        VulkanDeviceFunctions *vk = nullptr;
        VulkanDeviceFunctions  savedVk{};
        D3D12Functions        *dx = nullptr;
        D3D12Functions         savedDx;
    };

    class ResourceStates : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"ResourceStateTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            }
            info.deviceEpoch      = 151;
            info.parallelNum      = 2;
            info.trackedResources = 2;
            info.vulkanCompiler   = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
            buffer                  = BufferOf(MemoryType::GPU_ONLY);
            image.descriptor.format = PixelFormat::RGBA8_UNORM;
            image.descriptor.extent = {4, 4, 1};
            image.descriptor.usage  = ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST;
            image.image             = ImagePtr(device->CreateImage(image.descriptor));
            ASSERT_TRUE(buffer && image.image);
            ASSERT_TRUE(Ok(context->TrackResource(FrameResource{buffer}, CopyBufferState::COMMON)));
            ASSERT_TRUE(Ok(context->TrackResource(FrameResource{image.image.GetOwned()}, ImageLayout::UNDEFINED)));
        }
        void TearDown() override
        {
            context.reset();
            readbacks = {};
            buffer.Reset(nullptr);
            image.image = {};
            if (debug) {
                for (UINT64 i = 0; i < debug->GetNumStoredMessages(); ++i) {
                    SIZE_T size = 0;
                    debug->GetMessage(i, nullptr, &size);
                    std::vector<char> bytes(size);
                    auto             *message = reinterpret_cast<D3D12_MESSAGE *>(bytes.data());
                    if (SUCCEEDED(debug->GetMessage(i, message, &size)))
                        EXPECT_GT(message->Severity, D3D12_MESSAGE_SEVERITY_ERROR) << message->pDescription;
                }
            }
            debug.Reset();
            Instance::Destroy();
        }
        static bool Ok(const FrameOperationResult &result)
        {
            return std::holds_alternative<FrameOperationComplete>(result);
        }
        BufferPtr BufferOf(MemoryType memory)
        {
            Buffer::Descriptor desc;
            desc.size   = 64;
            desc.memory = memory;
            desc.usage  = BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::VERTEX;
            return BufferPtr(device->CreateBuffer(desc));
        }
        FrameResourceStateSnapshot State(const FrameResource &resource)
        {
            auto  result = context->GetResourceState(resource);
            auto *value  = std::get_if<FrameResourceStateSnapshot>(&result);
            EXPECT_TRUE(value);
            return value ? *value : FrameResourceStateSnapshot{};
        }
        void Initial()
        {
            const auto b = State(FrameResource{buffer}), i = State(FrameResource{image.image.GetOwned()});
            EXPECT_EQ(b.accepted.state, FrameResourceBoundaryState{CopyBufferState::COMMON});
            EXPECT_EQ(i.accepted.state, FrameResourceBoundaryState{ImageLayout::UNDEFINED});
            EXPECT_EQ(b.completed.state, b.accepted.state);
            EXPECT_EQ(i.completed.state, i.accepted.state);
            EXPECT_EQ(b.accepted.submission.serial, 0u);
            EXPECT_EQ(i.accepted.submission.serial, 0u);
        }
        DeviceFrameLease Begin()
        {
            auto  result = context->TryBeginSlot();
            auto *lease  = std::get_if<DeviceFrameLease>(&result);
            EXPECT_TRUE(lease);
            return lease ? *lease : DeviceFrameLease{};
        }
        std::shared_ptr<const FrozenFrameGraph> Freeze(FrameGraphBuilder &builder)
        {
            auto  result = builder.Freeze();
            auto *graph  = std::get_if<std::shared_ptr<const FrozenFrameGraph>>(&result);
            EXPECT_TRUE(graph);
            return graph ? *graph : nullptr;
        }
        std::shared_ptr<const FrozenFrameGraph> Graph(bool read = false)
        {
            FrameGraphBuilder builder;
            const auto        b = State(FrameResource{buffer}), i = State(FrameResource{image.image.GetOwned()});
            const auto        bh = std::get<GraphBufferHandle>(builder.Import(buffer, std::get<CopyBufferState>(b.accepted.state),
                                                                              read ? CopyBufferState::COMMON : CopyBufferState::RASTER_READ, read));
            auto              imported = image;
            imported.initial           = std::get<ImageBoundaryState>(i.accepted.state);
            imported.final             = read ? ImageLayout::GENERAL : ImageLayout::TRANSFER_SRC;
            imported.initialized       = read;
            const auto ih              = std::get<GraphImageHandle>(builder.Import(imported));
            for (uint32_t index = 0; index < 2; ++index) {
                auto host = BufferOf(read && GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY);
                EXPECT_TRUE(host);
                if (!read) {
                    auto *mapped = host->Map();
                    EXPECT_TRUE(mapped);
                    if (mapped)
                        std::memset(mapped, 0x39 + index, 64);
                    host->UnMap();
                } else {
                    readbacks[index] = host;
                }
                const auto state = read ? CopyBufferState::HOST_READ : CopyBufferState::HOST_WRITE;
                const auto hh    = std::get<GraphBufferHandle>(builder.Import(host, state, state, !read));
                if (index == 0) {
                    EXPECT_TRUE(Ok(builder.Copy({read ? bh : hh, read ? hh : bh, 0, 0, 64, true})));
                } else {
                    BufferImageCopy region;
                    region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
                    region.imageExtent = {4, 4, 1};
                    EXPECT_TRUE(
                        Ok(builder.Copy({hh, ih, region, read ? ImageCopyDirection::IMAGE_TO_BUFFER : ImageCopyDirection::BUFFER_TO_IMAGE, true})));
                }
            }
            return Freeze(builder);
        }
        std::unique_ptr<BackendCompiledFrame> Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
        {
            auto  result = context->Compile(lease, std::move(graph));
            auto *plan   = std::get_if<std::unique_ptr<BackendCompiledFrame>>(&result);
            if (!plan) {
                ADD_FAILURE() << "Compile failed: " << (std::get_if<FrameError>(&result) ? std::get<FrameError>(result).message : "no work");
                return {};
            }
            return std::move(*plan);
        }
        FrameCollectResult Collect(const EncodingTicket &ticket)
        {
            const auto deadline = std::chrono::steady_clock::now() + 5s;
            while (std::chrono::steady_clock::now() < deadline) {
                auto result = context->TryCollect(ticket);
                if (!std::holds_alternative<FramePending>(result))
                    return result;
                std::this_thread::sleep_for(1ms);
            }
            ADD_FAILURE() << "Encoding timed out";
            return FrameError{FrameErrorCode::INVALID_STATE};
        }
        std::unique_ptr<RecordedFrame> Record(std::unique_ptr<BackendCompiledFrame> plan)
        {
            auto  result = context->Dispatch(plan);
            auto *ticket = std::get_if<EncodingTicket>(&result);
            EXPECT_TRUE(ticket);
            if (!ticket)
                return {};
            auto  collected = Collect(*ticket);
            auto *recorded  = std::get_if<std::unique_ptr<RecordedFrame>>(&collected);
            EXPECT_TRUE(recorded);
            return recorded ? std::move(*recorded) : nullptr;
        }
        SubmissionToken Submit(std::unique_ptr<RecordedFrame> recorded)
        {
            auto  result   = context->Submit(recorded);
            auto *accepted = std::get_if<SubmissionAccepted>(&result);
            EXPECT_TRUE(accepted);
            return accepted ? accepted->token : SubmissionToken{};
        }
        FrameCompletion CompleteOne()
        {
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            std::array<FrameCompletion, 1> result;
            EXPECT_EQ(context->PollCompleted(result), 1u);
            EXPECT_EQ(result[0].status, FrameCompletionStatus::COMPLETE);
            return result[0];
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        BufferPtr                           buffer;
        FrameGraphImage                     image;
        std::array<BufferPtr, 2>            readbacks;
        ComPtr<ID3D12InfoQueue>             debug;
    };
} // namespace

TEST_P(ResourceStates, AcceptedStateFeedsTheNextGraphBeforeGpuCompletion)
{
    sky::aurora::Semaphore::Descriptor desc;
    desc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(desc));
    ASSERT_TRUE(gate);
    auto      *queue = device->GetQueue(QueueType::GRAPHICS);
    Finally    drain{[&] {
        if (gate->GetCurrentValue() < 1)
            gate->Signal(1);
        queue->WaitIdle();
    }};
    SubmitInfo wait;
    wait.waitSemaphores = {{gate.Get(), 1, PipelineStageBit::TOP}};
    ASSERT_EQ(queue->TrySubmit(wait).status, QueueSubmitStatus::ACCEPTED);
    auto plan = Compile(Begin(), Graph());
    ASSERT_TRUE(plan);
    Initial();
    auto recorded = Record(std::move(plan));
    ASSERT_TRUE(recorded);
    Initial();
    const auto first = Submit(std::move(recorded));
    EXPECT_EQ(State(FrameResource{buffer}).accepted.submission, first);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, first);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission.serial, 0u);
    const auto second = Submit(Record(Compile(Begin(), Graph(true))));
    EXPECT_GT(second.serial, first.serial);
    EXPECT_EQ(State(FrameResource{buffer}).accepted.submission, second);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.state, FrameResourceBoundaryState{ImageLayout::GENERAL});
    std::array<FrameCompletion, 2> completed;
    EXPECT_EQ(context->PollCompleted(completed), 0u);
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{buffer})));
    gate->Signal(1);
    queue->WaitIdle();
    EXPECT_EQ(context->PollCompleted(completed), 2u);
    EXPECT_EQ(State(FrameResource{buffer}).completed.submission, second);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission, second);
    for (uint32_t index = 0; index < readbacks.size(); ++index) {
        auto *mapped = readbacks[index]->Map();
        ASSERT_TRUE(mapped);
        for (uint32_t byte = 0; byte < 64; ++byte)
            EXPECT_EQ(mapped[byte], 0x39 + index);
        readbacks[index]->UnMap();
    }
    EXPECT_TRUE(Ok(context->UntrackResource(FrameResource{buffer})));
    EXPECT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
}

TEST_P(ResourceStates, CompletionPollingInSlotOrderNeverRegressesTheLedger)
{
    const auto first = Submit(Record(Compile(Begin(), Graph())));
    EXPECT_EQ(CompleteOne().token, first);
    const auto second = Submit(Record(Compile(Begin(), Graph(true))));
    const auto third  = Submit(Record(Compile(Begin(), Graph())));
    ASSERT_GT(third.serial, second.serial);
    EXPECT_EQ(CompleteOne().token, third);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission, third);
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    EXPECT_EQ(CompleteOne().token, second);
    EXPECT_EQ(State(FrameResource{buffer}).completed.submission, third);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission, third);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.state, FrameResourceBoundaryState{ImageLayout::TRANSFER_SRC});
    EXPECT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
}

TEST_P(ResourceStates, AbandonedPlansAndCancelledEncodingNeverPublishCandidates)
{
    const auto lease = Begin();
    auto       plan  = Compile(lease, Graph());
    ASSERT_TRUE(plan);
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{buffer})));
    EXPECT_FALSE(Ok(context->TrackResource(FrameResource{buffer}, CopyBufferState::COMMON)));
    plan.reset();
    Initial();
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
    const auto next = Begin();
    plan            = Compile(next, Graph());
    auto dispatch   = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatch));
    const auto ticket = std::get<EncodingTicket>(dispatch);
    EXPECT_TRUE(Ok(context->Cancel(ticket)));
    auto collected = Collect(ticket);
    EXPECT_TRUE(std::holds_alternative<EncodingCancelled>(collected));
    Initial();
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(next)));
}

TEST_P(ResourceStates, EncodingFailureLeavesBothSnapshotsUntouched)
{
    const auto   lease = Begin();
    auto         plan  = Compile(lease, Graph());
    LedgerFaults faults(*device);
    faults.EncodeFailure();
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto collected = Collect(std::get<EncodingTicket>(dispatched));
    EXPECT_TRUE(std::holds_alternative<EncodingFailed>(collected));
    faults.Restore();
    Initial();
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
}

TEST_P(ResourceStates, RejectedRecordingRetainsCandidateAndCanRetry)
{
    auto recorded = Record(Compile(Begin(), Graph()));
    ASSERT_TRUE(recorded);
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext(info));
    ASSERT_TRUE(other);
    EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(other->Submit(recorded)));
    ASSERT_TRUE(recorded);
    Initial();
    if (GetParam() != 2) {
        LedgerFaults faults(*device);
        faults.vk->vkQueueSubmit2 = [](VkQueue, uint32_t, const VkSubmitInfo2 *, VkFence) -> VkResult { return VK_ERROR_OUT_OF_HOST_MEMORY; };
        EXPECT_TRUE(std::holds_alternative<SubmissionRejected>(context->Submit(recorded)));
        ASSERT_TRUE(recorded);
        Initial();
    }
    const auto token = Submit(std::move(recorded));
    EXPECT_EQ(State(FrameResource{buffer}).accepted.submission, token);
    EXPECT_EQ(CompleteOne().token, token);
}

TEST_P(ResourceStates, StaleBufferAndImageImportsFailBeforeEncodingAndAllowRecompile)
{
    auto       stale    = Graph();
    const auto first    = Submit(Record(Compile(Begin(), stale)));
    const auto lease    = Begin();
    auto       compiled = context->Compile(lease, stale);
    ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
    EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_STATE);
    auto data               = stale->GetData();
    data.buffers[0].initial = CopyBufferState::RASTER_READ;
    auto imageStale         = std::make_shared<const FrozenFrameGraph>(stale->GetId(), std::move(data));
    compiled                = context->Compile(lease, imageStale);
    ASSERT_TRUE(std::holds_alternative<FrameError>(compiled));
    EXPECT_EQ(std::get<FrameError>(compiled).code, FrameErrorCode::INVALID_STATE);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, first);
    const auto second = Submit(Record(Compile(lease, Graph(true))));
    EXPECT_GT(second.serial, first.serial);
    CompleteOne();
    CompleteOne();
}

TEST_P(ResourceStates, NoWorkAndCulledImportsDoNotParticipateInTheLedger)
{
    const auto        first = Submit(Record(Compile(Begin(), Graph())));
    FrameGraphBuilder builder;
    EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(builder.Import(image)));
    auto lease = Begin();
    EXPECT_TRUE(std::holds_alternative<FrameNoWork>(context->Compile(lease, Freeze(builder))));
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, first);
    FrameGraphBuilder live;
    EXPECT_TRUE(std::holds_alternative<GraphImageHandle>(live.Import(image)));
    const auto bh = std::get<GraphBufferHandle>(live.Import(buffer, CopyBufferState::RASTER_READ, CopyBufferState::COMMON, true));
    EXPECT_TRUE(Ok(live.Export({bh, 0, 64})));
    const auto second = Submit(Record(Compile(Begin(), Freeze(live))));
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, first);
    EXPECT_EQ(State(FrameResource{buffer}).accepted.submission, second);
    EXPECT_EQ(CompleteOne().token, first);
    EXPECT_EQ(CompleteOne().token, second);
}

TEST_P(ResourceStates, TrackingIsBoundedTypedAndPinsExactBackingUntilRelease)
{
    EXPECT_FALSE(Ok(context->TrackResource(FrameResource{BufferPtr{}}, CopyBufferState::COMMON)));
    EXPECT_FALSE(Ok(context->TrackResource(FrameResource{buffer}, ImageLayout::GENERAL)));
    EXPECT_FALSE(Ok(context->TrackResource(FrameResource{buffer}, CopyBufferState::COMMON)));
    BufferPtr unowned(new Buffer());
    EXPECT_FALSE(Ok(context->TrackResource(FrameResource{unowned}, CopyBufferState::COMMON)));
    auto       extra = BufferOf(MemoryType::GPU_ONLY);
    const auto full  = context->TrackResource(FrameResource{extra}, CopyBufferState::COMMON);
    ASSERT_TRUE(std::holds_alternative<FrameError>(full));
    EXPECT_EQ(std::get<FrameError>(full).code, FrameErrorCode::OUT_OF_MEMORY);
    const auto references = buffer->GetRef();
    EXPECT_TRUE(Ok(context->UntrackResource(FrameResource{buffer})));
    EXPECT_EQ(buffer->GetRef() + 1, references);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{buffer})));
    EXPECT_TRUE(Ok(context->TrackResource(FrameResource{extra}, CopyBufferState::COMMON)));
    std::unique_ptr<DeviceFrameContext> other(device->CreateFrameContext(info));
    EXPECT_TRUE(std::holds_alternative<FrameError>(other->GetResourceState(FrameResource{extra})));
    const auto lease   = Begin();
    auto      *backend = static_cast<BackendFrameContext *>(context.get());
    EXPECT_TRUE(std::holds_alternative<FrameError>(backend->AllocateCommand(lease, 0)));
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
}

TEST_P(ResourceStates, StateTrackingDoesNotCreateInitializedContent)
{
    const auto first = Submit(Record(Compile(Begin(), Graph())));
    EXPECT_EQ(CompleteOne().token, first);
    FrameGraphBuilder builder;
    auto              imported = image;
    imported.initial           = ImageLayout::TRANSFER_SRC;
    imported.final             = ImageLayout::GENERAL;
    const auto ih              = std::get<GraphImageHandle>(builder.Import(imported));
    EXPECT_TRUE(Ok(builder.Export(ih)));
    const auto lease = Begin();
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->Compile(lease, Freeze(builder))));
    EXPECT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, first);
}

TEST_P(ResourceStates, IndeterminateSubmissionInvalidatesAllLedgerQueries)
{
    auto         recorded = Record(Compile(Begin(), Graph()));
    LedgerFaults faults(*device);
    faults.UncertainSubmit();
    EXPECT_TRUE(std::holds_alternative<SubmissionIndeterminate>(context->Submit(recorded)));
    EXPECT_FALSE(recorded);
    faults.Restore();
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{buffer})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{image.image.GetOwned()})));
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->TryBeginSlot()));
}

TEST_P(ResourceStates, FailedCompletionInvalidatesTheEpochWithoutReleasingPins)
{
    Submit(Record(Compile(Begin(), Graph())));
    device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    const auto   references = image.image->GetRef();
    LedgerFaults faults(*device);
    faults.QueryFailure();
    std::array<FrameCompletion, 2> completed;
    ASSERT_EQ(context->PollCompleted(completed), 1u);
    EXPECT_EQ(completed[0].status, FrameCompletionStatus::DEVICE_LOST);
    faults.Restore();
    EXPECT_EQ(image.image->GetRef(), references);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{buffer})));
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
}

INSTANTIATE_TEST_SUITE_P(NativeCompilers, ResourceStates, testing::Values(0u, 1u, 2u));

TEST_P(ResourceStates, OwnedTrackingExpiresOnCoordinatorAndNeverUntracksALiveOwner)
{
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{buffer})));
    auto result = context->TrackOwnedResource(FrameResource{buffer}, CopyBufferState::COMMON);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(result));
    auto owner = std::move(std::get<std::shared_ptr<const FrameResourceTracking>>(result));
    auto copy  = owner;
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{buffer})));
    std::thread release([owner = std::move(owner)]() mutable { owner.reset(); });
    release.join();
    EXPECT_EQ(context->PollCompleted({}), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameResourceStateSnapshot>(context->GetResourceState(FrameResource{buffer})));
    std::thread last([copy = std::move(copy)]() mutable { copy.reset(); });
    last.join();
    EXPECT_TRUE(std::holds_alternative<FrameResourceStateSnapshot>(context->GetResourceState(FrameResource{buffer})));
    EXPECT_EQ(context->PollCompleted({}), 0u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{buffer})));
    result = context->TrackOwnedResource(FrameResource{buffer}, CopyBufferState::COMMON);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(result));
    owner = std::move(std::get<std::shared_ptr<const FrameResourceTracking>>(result));
    context.reset();
    owner.reset();
}

TEST_P(ResourceStates, DroppedOwnedTrackingSurvivesPlanAndSubmissionUntilRetirement)
{
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{buffer})));
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    auto b = context->TrackOwnedResource(FrameResource{buffer}, CopyBufferState::COMMON);
    auto i = context->TrackOwnedResource(FrameResource{image.image.GetOwned()}, ImageLayout::UNDEFINED);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(b));
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(i));
    auto plan = Compile(Begin(), Graph());
    ASSERT_TRUE(plan);
    b = FrameError{};
    i = FrameError{};
    EXPECT_EQ(context->PollCompleted({}), 0u);
    Initial();
    const auto token = Submit(Record(std::move(plan)));
    EXPECT_EQ(context->PollCompleted({}), 0u);
    EXPECT_EQ(State(FrameResource{buffer}).accepted.submission, token);
    EXPECT_EQ(CompleteOne().token, token);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{buffer})));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{image.image.GetOwned()})));
}

TEST_P(ResourceStates, ExpiredOwnedTrackingCannotReleaseQuarantinedResources)
{
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{buffer})));
    auto owner = context->TrackOwnedResource(FrameResource{buffer}, CopyBufferState::COMMON);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(owner));
    auto recorded = Record(Compile(Begin(), Graph()));
    owner         = FrameError{};
    LedgerFaults faults(*device);
    faults.UncertainSubmit();
    ASSERT_TRUE(std::holds_alternative<SubmissionIndeterminate>(context->Submit(recorded)));
    faults.Restore();
    const auto references = buffer->GetRef();
    EXPECT_GT(references, 1u);
    EXPECT_EQ(context->PollCompleted({}), 0u);
    EXPECT_EQ(buffer->GetRef(), references);
    context.reset();
    EXPECT_EQ(buffer->GetRef(), 1u);
}

TEST_P(ResourceStates, MixedAcceptedLayoutsRejectOneStaleCellAndFeedAnInflightSuccessor)
{
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    image.descriptor.mipLevels   = 3;
    image.descriptor.arrayLayers = 2;
    image.image                  = ImagePtr(device->CreateImage(image.descriptor));
    ASSERT_TRUE(image.image);
    const ImageBoundaryState first{{ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST, ImageLayout::TRANSFER_DST,
                                    ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC}};
    const ImageBoundaryState second{{ImageLayout::TRANSFER_DST, ImageLayout::GENERAL, ImageLayout::TRANSFER_SRC, ImageLayout::GENERAL,
                                     ImageLayout::TRANSFER_SRC, ImageLayout::TRANSFER_DST}};
    ASSERT_TRUE(
        Ok(context->TrackResource(FrameResource{image.image.GetOwned()}, ImageBoundaryState{std::vector<ImageLayout>(6, ImageLayout::UNDEFINED)})));
    auto  upload = BufferOf(MemoryType::CPU_ONLY), output = BufferOf(MemoryType::GPU_TO_CPU);
    auto *bytes = upload->Map();
    ASSERT_TRUE(bytes);
    std::fill_n(bytes, 4, uint8_t{93});
    upload->UnMap();
    auto graph = [&](bool read) {
        FrameGraphBuilder builder;
        auto              imported = image;
        imported.initial           = read ? first : ImageBoundaryState{ImageLayout::UNDEFINED};
        imported.final             = read ? second : first;
        if (read)
            imported.initializedRanges = {{2, 1, 1, 1, AspectFlagBit::COLOR_BIT}};
        const auto      ih    = std::get<GraphImageHandle>(builder.Import(imported));
        const auto      state = read ? CopyBufferState::HOST_READ : CopyBufferState::HOST_WRITE;
        const auto      bh    = std::get<GraphBufferHandle>(builder.Import(read ? output : upload, state, state, !read));
        BufferImageCopy region;
        region.subRange    = {2, 1, 1, AspectFlagBit::COLOR_BIT};
        region.imageExtent = {1, 1, 1};
        EXPECT_TRUE(Ok(builder.Copy({bh, ih, region, read ? ImageCopyDirection::IMAGE_TO_BUFFER : ImageCopyDirection::BUFFER_TO_IMAGE, true})));
        return Freeze(builder);
    };
    const auto abandonedLease = Begin();
    auto       abandoned      = Compile(abandonedLease, graph(false));
    ASSERT_TRUE(abandoned);
    abandoned.reset();
    ASSERT_TRUE(Ok(context->DiscardUnsubmitted(abandonedLease)));
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission.serial, 0u);
    sky::aurora::Semaphore::Descriptor desc;
    desc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(desc));
    ASSERT_TRUE(gate);
    auto      *queue = device->GetQueue(QueueType::GRAPHICS);
    Finally    drain{[&] {
        if (gate->GetCurrentValue() < 1)
            gate->Signal(1);
        queue->WaitIdle();
    }};
    SubmitInfo wait;
    wait.waitSemaphores = {{gate.Get(), 1, PipelineStageBit::TOP}};
    ASSERT_EQ(queue->TrySubmit(wait).status, QueueSubmitStatus::ACCEPTED);
    const auto a = Submit(Record(Compile(Begin(), graph(false))));
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.state, FrameResourceBoundaryState{first});
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.state, FrameResourceBoundaryState{ImageLayout::UNDEFINED});
    const auto nextLease                                                       = Begin();
    const auto next                                                            = graph(true);
    auto       data                                                            = next->GetData();
    std::get<std::vector<ImageLayout>>(data.images.front().initial.layouts)[0] = ImageLayout::TRANSFER_SRC;
    const auto rejected = context->Compile(nextLease, std::make_shared<const FrozenFrameGraph>(next->GetId(), std::move(data)));
    ASSERT_TRUE(std::holds_alternative<FrameError>(rejected));
    EXPECT_EQ(std::get<FrameError>(rejected).code, FrameErrorCode::INVALID_STATE);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.submission, a);
    const auto b = Submit(Record(Compile(nextLease, next)));
    EXPECT_GT(b.serial, a.serial);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).accepted.state, FrameResourceBoundaryState{second});
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission.serial, 0u);
    std::array<FrameCompletion, 2> completed;
    EXPECT_EQ(context->PollCompleted(completed), 0u);
    EXPECT_FALSE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    gate->Signal(1);
    queue->WaitIdle();
    ASSERT_EQ(context->PollCompleted(completed), 2u);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.submission, b);
    EXPECT_EQ(State(FrameResource{image.image.GetOwned()}).completed.state, FrameResourceBoundaryState{second});
    bytes = output->Map();
    ASSERT_TRUE(bytes);
    EXPECT_TRUE(std::all_of(bytes, bytes + 4, [](uint8_t value) { return value == 93; }));
    output->UnMap();
    EXPECT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
}

TEST_P(ResourceStates, ImageCellBudgetValidatesActualStorageAndReclaimsBothOwnershipModes)
{
    context.reset();
    info.trackedResources         = 8;
    info.trackedImageSubresources = 6;
    context.reset(device->CreateFrameContext(info));
    ASSERT_TRUE(context);
    auto desc        = image.descriptor;
    desc.mipLevels   = 3;
    desc.arrayLayers = 2;
    ImagePtr large(device->CreateImage(desc));
    ASSERT_TRUE(large);
    EXPECT_EQ(large->GetMipLayerCount(), 6u);
    EXPECT_TRUE(std::holds_alternative<FrameError>(
        context->TrackResource(FrameResource{large}, ImageBoundaryState{std::vector<ImageLayout>(5, ImageLayout::UNDEFINED)})));
    auto result = context->TrackOwnedResource(FrameResource{large}, ImageLayout::UNDEFINED);
    ASSERT_TRUE(std::holds_alternative<std::shared_ptr<const FrameResourceTracking>>(result));
    auto       owner = std::get<std::shared_ptr<const FrameResourceTracking>>(std::move(result));
    const auto full  = context->TrackResource(FrameResource{image.image.GetOwned()}, ImageLayout::UNDEFINED);
    ASSERT_TRUE(std::holds_alternative<FrameError>(full));
    EXPECT_EQ(std::get<FrameError>(full).code, FrameErrorCode::OUT_OF_MEMORY);
    const auto        lease = Begin();
    FrameGraphBuilder builder;
    FrameGraphImage   imported{large, desc, ImageLayout::UNDEFINED, ImageLayout::GENERAL, false};
    const auto        ih   = std::get<GraphImageHandle>(builder.Import(imported));
    auto              host = BufferOf(MemoryType::CPU_ONLY);
    const auto        bh   = std::get<GraphBufferHandle>(builder.Import(host, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
    BufferImageCopy   region;
    region.subRange    = {2, 1, 1, AspectFlagBit::COLOR_BIT};
    region.imageExtent = {1, 1, 1};
    ASSERT_TRUE(Ok(builder.Copy({bh, ih, region, ImageCopyDirection::BUFFER_TO_IMAGE, true})));
    auto plan = Compile(lease, Freeze(builder));
    ASSERT_TRUE(plan);
    owner.reset();
    context->PollCompleted({});
    EXPECT_TRUE(std::holds_alternative<FrameResourceStateSnapshot>(context->GetResourceState(FrameResource{large})));
    plan.reset();
    ASSERT_TRUE(Ok(context->DiscardUnsubmitted(lease)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->GetResourceState(FrameResource{large})));
    ASSERT_TRUE(Ok(context->TrackResource(FrameResource{large}, ImageLayout::UNDEFINED)));
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{large})));
    ASSERT_TRUE(Ok(context->TrackResource(FrameResource{image.image.GetOwned()}, ImageLayout::UNDEFINED)));
    const auto stillFull = context->TrackResource(FrameResource{large}, ImageLayout::UNDEFINED);
    ASSERT_TRUE(std::holds_alternative<FrameError>(stillFull));
    EXPECT_EQ(std::get<FrameError>(stillFull).code, FrameErrorCode::OUT_OF_MEMORY);
    ASSERT_TRUE(Ok(context->UntrackResource(FrameResource{image.image.GetOwned()})));
    ASSERT_TRUE(Ok(context->TrackResource(FrameResource{large}, ImageLayout::UNDEFINED)));
}
