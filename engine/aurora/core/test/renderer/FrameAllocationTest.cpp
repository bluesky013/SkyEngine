//
// Created on 2026/10/16.
//

#include "GpuTestSupport.h"
#include "shaders/FrameGraphSPIRV.h"

#include "graph/FrameGraphBuilder.h"
#include <D3D12Device.h>
#include <d3d12sdklayers.h>
#include <rdg/D3D12DeviceFrameContext.h>
#include <rdg/VulkanDeviceFrameContext.h>

namespace {
    VkResult           allocationFault = VK_SUCCESS;
    VkResult VKAPI_PTR FailPoolCreate(VkDevice, const VkDescriptorPoolCreateInfo *, const VkAllocationCallbacks *, VkDescriptorPool *)
    {
        return allocationFault;
    }
    VkResult VKAPI_PTR FailSetAllocate(VkDevice, const VkDescriptorSetAllocateInfo *, VkDescriptorSet *)
    {
        return allocationFault;
    }
    VkResult VKAPI_PTR FailPoolReset(VkDevice, VkDescriptorPool, VkDescriptorPoolResetFlags)
    {
        return allocationFault;
    }
    HRESULT FailHeapCreate(ID3D12Device *, const D3D12_DESCRIPTOR_HEAP_DESC *, ID3D12DescriptorHeap **)
    {
        return allocationFault == VK_ERROR_DEVICE_LOST ? DXGI_ERROR_DEVICE_REMOVED : E_OUTOFMEMORY;
    }
    class FrameAllocations : public testing::TestWithParam<uint32_t> {
    protected:
        void SetUp() override
        {
            Instance::Get()->Init({"FrameAllocationTest", "SkyEngine", true, GetParam() == 2 ? API::DX12 : API::VULKAN});
            device = Instance::Get()->GetDevice();
            ASSERT_TRUE(device);
            if (GetParam() == 2) {
                ASSERT_TRUE(SUCCEEDED(static_cast<D3D12Device *>(device)->GetNativeHandle()->QueryInterface(IID_PPV_ARGS(&debug))));
                debug->ClearStoredMessages();
            } else {
                auto function = [&](ShaderStageFlagBit stage, std::span<const uint32_t> words) {
                    CounterPtr<ShaderBinaryProvider> provider(new ShaderBinaryProvider());
                    provider->binaryData = BinaryDataPtr(new BinaryData(static_cast<uint32_t>(words.size_bytes())));
                    std::memcpy(provider->binaryData->Data(), words.data(), words.size_bytes());
                    return ShaderFunctionPtr(device->CreateShaderFunction({stage, provider, {}}));
                };
                auto             vs = function(ShaderStageFlagBit::VS, FrameGraphVS);
                auto             fs = function(ShaderStageFlagBit::FS, FrameGraphRedFS);
                ShaderReflection reflection;
                reflection.resources = {{"view", ShaderResourceType::UNIFORM_BUFFER, 0, 0, 1}, {"sampler", ShaderResourceType::SAMPLER, 1, 0, 1}};
                Shader::Descriptor desc{};
                desc.vs         = vs.Get();
                desc.ps         = fs.Get();
                desc.reflection = &reflection;
                shader          = device->CreateShader(desc);
                ASSERT_TRUE(shader);
            }
            info.parallelNum                     = 2;
            info.vulkanCompiler                  = GetParam() == 1 ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
            info.allocations.stagingBytes        = 96;
            info.allocations.uniformBytes        = 2 * device->GetCapability().minUniformBufferOffsetAlignment;
            info.allocations.resourceDescriptors = 2;
            info.allocations.samplerDescriptors  = 1;
            info.allocations.descriptorSets      = 3;
            Reset();
        }
        void TearDown() override
        {
            context.reset();
            shader.Reset(nullptr);
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
        void Reset()
        {
            context.reset(device->CreateFrameContext(info));
            ASSERT_TRUE(context);
        }
        DeviceFrameLease Begin()
        {
            auto result = context->TryBeginSlot();
            if (auto *lease = std::get_if<DeviceFrameLease>(&result))
                return *lease;
            ADD_FAILURE() << "Begin failed";
            return {};
        }
        FrameBufferSlice
        Write(const DeviceFrameLease &lease, std::span<const std::byte> data, FrameBufferKind kind = FrameBufferKind::STAGING, uint64_t alignment = 1)
        {
            auto result = context->WriteFrameBuffer(lease, kind, data, alignment);
            if (auto *slice = std::get_if<FrameBufferSlice>(&result))
                return std::move(*slice);
            ADD_FAILURE() << "Write failed";
            return {};
        }
        std::variant<FrameError, uint64_t> Descriptor(const DeviceFrameLease &lease, bool sampler = false)
        {
            if (GetParam() == 2) {
                auto result =
                    static_cast<D3D12DeviceFrameContext *>(context.get())
                        ->AllocateDescriptors(lease, sampler ? D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER : D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
                if (auto *error = std::get_if<FrameError>(&result))
                    return *error;
                const auto &range = std::get<D3D12FrameDescriptors>(result);
                EXPECT_NE(range.cpu.ptr, 0u);
                EXPECT_NE(range.gpu.ptr, 0u);
                return range.gpu.ptr;
            }
            auto result = static_cast<VulkanDeviceFrameContext *>(context.get())->AllocateDescriptorSet(lease, *shader, sampler ? 1 : 0);
            if (auto *error = std::get_if<FrameError>(&result))
                return *error;
            return reinterpret_cast<uint64_t>(std::get<VkDescriptorSet>(result));
        }
        BufferPtr Readback()
        {
            Buffer::Descriptor desc;
            desc.size   = 64;
            desc.memory = GetParam() == 2 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
            desc.usage  = BufferUsageFlagBit::TRANSFER_DST;
            return BufferPtr(device->CreateBuffer(desc));
        }
        std::shared_ptr<const FrozenFrameGraph> CopyGraph(const FrameBufferSlice &slice, BufferPtr readback, bool exported = true)
        {
            FrameGraphBuilder builder;
            const auto        src =
                std::get<GraphBufferHandle>(builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true));
            const auto dst = std::get<GraphBufferHandle>(builder.Import(readback, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Copy({src, dst, slice.offset, 0, 64})));
            if (exported)
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(builder.Export({dst, 0, 64})));
            return std::get<std::shared_ptr<const FrozenFrameGraph>>(builder.Freeze());
        }
        void Complete(size_t expected)
        {
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
            std::array<FrameCompletion, 2> results;
            const auto                     count = context->PollCompleted(results);
            ASSERT_EQ(count, expected);
            for (size_t i = 0; i < count; ++i)
                EXPECT_EQ(results[i].status, FrameCompletionStatus::COMPLETE);
        }
        Device                             *device = nullptr;
        DeviceFrameContextInitInfo          info;
        std::unique_ptr<DeviceFrameContext> context;
        ShaderPtr                           shader;
        ComPtr<ID3D12InfoQueue>             debug;
    };
} // namespace

TEST_P(FrameAllocations, CheckedRangesAndViewConstantsShareOneLease)
{
    const auto                lease = Begin();
    std::array<std::byte, 96> data;
    data.fill(std::byte{0x35});
    auto first = Write(lease, std::span(data).first(33), FrameBufferKind::STAGING, 16);
    ASSERT_TRUE(first.buffer);
    EXPECT_EQ(first.offset, 0u);
    EXPECT_EQ(std::get<uint64_t>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING, 16)), 48u);
    EXPECT_EQ(std::get<FrameError>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING, 3)).code, FrameErrorCode::INVALID_ARGUMENT);
    auto wrong = lease;
    ++wrong.context.contextId;
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(wrong, FrameBufferKind::STAGING, data)).code, FrameErrorCode::WRONG_CONTEXT);
    EXPECT_EQ(std::get<FrameError>(context->AvailableFrameBufferBytes(wrong, FrameBufferKind::STAGING)).code, FrameErrorCode::WRONG_CONTEXT);
    ++wrong.generation;
    wrong.context = lease.context;
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(wrong, FrameBufferKind::STAGING, data)).code, FrameErrorCode::STALE_LEASE);
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, data, 3)).code, FrameErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, {})).code, FrameErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, data, uint64_t{1} << 63)).code,
              FrameErrorCode::OUT_OF_MEMORY);
    auto second = Write(lease, std::span(data).first(31), FrameBufferKind::STAGING, 16);
    EXPECT_EQ(second.buffer.Get(), first.buffer.Get());
    EXPECT_EQ(second.offset, 48u);
    EXPECT_EQ(std::get<uint64_t>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING)), 17u);
    EXPECT_EQ(std::get<uint64_t>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING, 16)), 16u);
    EXPECT_EQ(Write(lease, std::span(data).first(17)).offset, 79u);
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, data)).code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_EQ(std::get<uint64_t>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING)), 0u);
    const auto alignment = device->GetCapability().minUniformBufferOffsetAlignment;
    for (uint32_t view = 0; view < 2; ++view) {
        auto constants = Write(lease, std::span(data).first(13), FrameBufferKind::UNIFORM);
        ASSERT_TRUE(constants.buffer);
        EXPECT_EQ(constants.lease, lease);
        EXPECT_EQ(constants.offset, view * alignment);
        EXPECT_EQ(constants.size, alignment);
        EXPECT_EQ(std::get<uint64_t>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::UNIFORM)), (1 - view) * alignment);
        const auto *mapped = constants.buffer->Map() + constants.offset;
        EXPECT_EQ(std::memcmp(mapped, data.data(), 13), 0);
        for (uint32_t i = 13; i < alignment; ++i)
            EXPECT_EQ(mapped[i], 0u);
    }
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::UNIFORM, data)).code, FrameErrorCode::OUT_OF_MEMORY);
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
}

TEST_P(FrameAllocations, UploadOnlyGraphFreezesAllocationsAndOwnsCopiedPayload)
{
    auto                      lease = Begin();
    std::array<std::byte, 64> data;
    data.fill(std::byte{0x59});
    auto slice    = Write(lease, data);
    auto readback = Readback();
    ASSERT_TRUE(slice.buffer && readback);
    data.fill(std::byte{0});
    auto compiled = context->Compile(lease, CopyGraph(slice, readback));
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<BackendCompiledFrame>>(compiled));
    auto plan = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
    EXPECT_EQ(std::get<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, data)).code, FrameErrorCode::INVALID_STATE);
    EXPECT_EQ(std::get<FrameError>(context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING)).code, FrameErrorCode::INVALID_STATE);
    EXPECT_EQ(std::get<FrameError>(Descriptor(lease)).code, FrameErrorCode::INVALID_STATE);
    auto dispatched = context->Dispatch(plan);
    ASSERT_TRUE(std::holds_alternative<EncodingTicket>(dispatched));
    auto       ticket    = std::get<EncodingTicket>(dispatched);
    auto       collected = context->TryCollect(ticket);
    const auto deadline  = std::chrono::steady_clock::now() + 5s;
    while (std::holds_alternative<FramePending>(collected) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
        collected = context->TryCollect(ticket);
    }
    ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(collected));
    auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(collected));
    ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->WriteFrameBuffer(lease, FrameBufferKind::STAGING, data)));
    Complete(1);
    data.fill(std::byte{0x59});
    ASSERT_TRUE(readback->Map());
    EXPECT_EQ(std::memcmp(readback->Map(), data.data(), data.size()), 0);
    readback->UnMap();
}

TEST_P(FrameAllocations, TwoInflightSlotsSurviveEmptyAndSkippedOutputs)
{
    sky::aurora::Semaphore::Descriptor desc;
    desc.type = SemaphoreType::TIMELINE;
    SemaphorePtr gate(device->CreateSema(desc));
    ASSERT_TRUE(gate);
    Finally                         drain{[&] {
        if (gate->GetCurrentValue() < 1)
            gate->Signal(1);
        device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    }};
    std::array<BufferPtr, 2>        readbacks;
    std::array<FrameBufferSlice, 2> slices;
    std::array<uint64_t, 2>         descriptors{};
    std::array<std::byte, 64>       data;
    auto                           *backend = static_cast<BackendFrameContext *>(context.get());
    for (uint32_t frame = 0; frame < 2; ++frame) {
        const auto lease = Begin();
        data.fill(static_cast<std::byte>(frame + 31));
        slices[frame]    = Write(lease, data);
        readbacks[frame] = Readback();
        ASSERT_TRUE(slices[frame].buffer && readbacks[frame]);
        auto allocated = Descriptor(lease);
        ASSERT_TRUE(std::holds_alternative<uint64_t>(allocated));
        descriptors[frame] = std::get<uint64_t>(allocated);
        auto commandResult = backend->AllocateCommand(lease, 0);
        ASSERT_TRUE(std::holds_alternative<CommandBuffer *>(commandResult));
        auto *command = std::get<CommandBuffer *>(commandResult);
        command->Begin();
        command->CreateBlitEncoder()->CopyBuffer(slices[frame].buffer.Get(), readbacks[frame].Get(), 64, slices[frame].offset, 0);
        HostReadBarrier(device, command);
        command->End();
        FrameRecordingInputs inputs;
        inputs.waits     = {{gate.Get(), 1, PipelineStageBit::TRANSFER}};
        inputs.resources = {slices[frame].buffer, readbacks[frame]};
        auto finished    = backend->FinishRecording(lease, std::move(inputs));
        ASSERT_TRUE(std::holds_alternative<std::unique_ptr<RecordedFrame>>(finished));
        auto recording = std::move(std::get<std::unique_ptr<RecordedFrame>>(finished));
        ASSERT_TRUE(std::holds_alternative<SubmissionAccepted>(context->Submit(recording)));
        if (frame == 0) {
            for (uint32_t skip = 0; skip < 5; ++skip) {
                const auto idle = Begin();
                EXPECT_NE(idle.slot, lease.slot);
                if (skip & 1) {
                    const auto unused = Write(idle, data);
                    EXPECT_TRUE(std::holds_alternative<FrameNoWork>(context->Compile(idle, CopyGraph(unused, readbacks[1] = Readback(), false))));
                } else {
                    FrameGraphBuilder empty;
                    EXPECT_TRUE(std::holds_alternative<FrameNoWork>(
                        context->Compile(idle, std::get<std::shared_ptr<const FrozenFrameGraph>>(empty.Freeze()))));
                }
                EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(idle)));
            }
        }
    }
    EXPECT_NE(slices[0].buffer.Get(), slices[1].buffer.Get());
    EXPECT_NE(descriptors[0], descriptors[1]);
    EXPECT_TRUE(std::holds_alternative<FrameBusy>(context->TryBeginSlot()));
    std::array<FrameCompletion, 2> pending;
    EXPECT_EQ(context->PollCompleted(pending), 0u);
    gate->Signal(1);
    Complete(2);
    for (uint32_t i = 0; i < 2; ++i) {
        data.fill(static_cast<std::byte>(i + 31));
        ASSERT_TRUE(readbacks[i]->Map());
        EXPECT_EQ(std::memcmp(readbacks[i]->Map(), data.data(), data.size()), 0);
        readbacks[i]->UnMap();
    }
    auto reused = Begin();
    EXPECT_GT(reused.generation, slices[0].lease.generation);
    EXPECT_TRUE(std::holds_alternative<FrameError>(context->WriteFrameBuffer(slices[reused.slot].lease, FrameBufferKind::STAGING, data)));
    EXPECT_EQ(Write(reused, data).offset, 0u);
    EXPECT_TRUE(std::holds_alternative<uint64_t>(Descriptor(reused)));
    EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(reused)));
}

TEST_P(FrameAllocations, DescriptorBudgetsResetOnlyWithANewLease)
{
    info.inflightNum = 1;
    Reset();
    DeviceFrameLease previous;
    for (uint32_t round = 0; round < 3; ++round) {
        auto lease = Begin();
        EXPECT_GT(lease.generation, previous.generation);
        if (previous.generation)
            EXPECT_EQ(std::get<FrameError>(Descriptor(previous)).code, FrameErrorCode::STALE_LEASE);
        auto first = Descriptor(lease), second = Descriptor(lease);
        ASSERT_TRUE(std::holds_alternative<uint64_t>(first) && std::holds_alternative<uint64_t>(second));
        EXPECT_NE(std::get<uint64_t>(first), std::get<uint64_t>(second));
        EXPECT_EQ(std::get<FrameError>(Descriptor(lease)).code, FrameErrorCode::OUT_OF_MEMORY);
        EXPECT_TRUE(std::holds_alternative<uint64_t>(Descriptor(lease, true)));
        EXPECT_EQ(std::get<FrameError>(Descriptor(lease, true)).code, FrameErrorCode::OUT_OF_MEMORY);
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        previous = lease;
    }
}

INSTANTIATE_TEST_SUITE_P(NativeModes, FrameAllocations, testing::Values(0u, 1u, 2u));

TEST_P(FrameAllocations, NativeDescriptorFailuresPreserveBudgetsAndCloseLostContexts)
{
    info.inflightNum = 1;
    for (bool lost : {false, true}) {
        Reset();
        const auto lease = Begin();
        allocationFault  = lost ? VK_ERROR_DEVICE_LOST : VK_ERROR_OUT_OF_HOST_MEMORY;
        if (GetParam() == 2) {
            auto      &fn    = const_cast<D3D12Functions &>(static_cast<D3D12Device *>(device)->GetFunctions());
            const auto saved = fn.createDescriptorHeap;
            Finally    restore{[&] { fn.createDescriptorHeap = saved; }};
            fn.createDescriptorHeap = FailHeapCreate;
            const auto result       = Descriptor(lease);
            ASSERT_TRUE(std::holds_alternative<FrameError>(result));
            const auto &error = std::get<FrameError>(result);
            EXPECT_EQ(error.cause.code, lost ? GpuErrorCode::DEVICE_LOST : GpuErrorCode::OUT_OF_MEMORY);
            EXPECT_EQ(error.cause.nativeCode, lost ? DXGI_ERROR_DEVICE_REMOVED : E_OUTOFMEMORY);
        } else {
            auto      &fn    = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
            const auto saved = fn.vkCreateDescriptorPool;
            Finally    restore{[&] { fn.vkCreateDescriptorPool = saved; }};
            fn.vkCreateDescriptorPool = FailPoolCreate;
            const auto result         = Descriptor(lease);
            ASSERT_TRUE(std::holds_alternative<FrameError>(result));
            EXPECT_EQ(std::get<FrameError>(result).cause.nativeCode, allocationFault);
        }
        if (!lost) {
            EXPECT_TRUE(std::holds_alternative<uint64_t>(Descriptor(lease)));
            if (GetParam() != 2) {
                auto      &fn    = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
                const auto saved = fn.vkAllocateDescriptorSets;
                Finally    restore{[&] { fn.vkAllocateDescriptorSets = saved; }};
                fn.vkAllocateDescriptorSets = FailSetAllocate;
                EXPECT_EQ(std::get<FrameError>(Descriptor(lease)).cause.nativeCode, allocationFault);
            }
            EXPECT_TRUE(std::holds_alternative<uint64_t>(Descriptor(lease)));
            EXPECT_EQ(std::get<FrameError>(Descriptor(lease)).code, FrameErrorCode::OUT_OF_MEMORY);
        } else {
            EXPECT_EQ(std::get<FrameError>(Descriptor(lease)).code, FrameErrorCode::INVALID_STATE);
        }
        EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(lease)));
        if (lost)
            EXPECT_EQ(std::get<FrameError>(context->TryBeginSlot()).code, FrameErrorCode::INVALID_STATE);
        else if (GetParam() != 2) {
            auto      &fn    = const_cast<VulkanDeviceFunctions &>(static_cast<VulkanDevice *>(device)->GetDeviceFn());
            const auto saved = fn.vkResetDescriptorPool;
            {
                Finally restore{[&] { fn.vkResetDescriptorPool = saved; }};
                fn.vkResetDescriptorPool = FailPoolReset;
                EXPECT_EQ(std::get<FrameError>(context->TryBeginSlot()).cause.nativeCode, allocationFault);
            }
            const auto next = Begin();
            EXPECT_EQ(next.generation, lease.generation + 1);
            EXPECT_TRUE(std::holds_alternative<uint64_t>(Descriptor(next)));
            EXPECT_TRUE(std::holds_alternative<FrameOperationComplete>(context->DiscardUnsubmitted(next)));
            allocationFault = VK_ERROR_DEVICE_LOST;
            Finally restore{[&] { fn.vkResetDescriptorPool = saved; }};
            fn.vkResetDescriptorPool = FailPoolReset;
            EXPECT_EQ(std::get<FrameError>(context->TryBeginSlot()).cause.nativeCode, allocationFault);
            EXPECT_EQ(std::get<FrameError>(context->TryBeginSlot()).code, FrameErrorCode::INVALID_STATE);
        }
    }
}
