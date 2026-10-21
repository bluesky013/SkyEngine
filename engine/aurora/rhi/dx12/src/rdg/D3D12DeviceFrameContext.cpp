
#include <rdg/D3D12DeviceFrameContext.h>

#include "D3D12FrameCompiler.h"
#include <D3D12Device.h>
#include <algorithm>
#include <array>
#include <core/async/TaskDispatcher.h>

namespace sky::aurora {
    namespace {
        struct D3D12EncodeWorker final : ThreadContext {
            explicit D3D12EncodeWorker(uint32_t index) : index(index)
            {
            }
            uint32_t index;
        };
        struct WorkerPartition {
            std::unique_ptr<D3D12CommandPool> pool;
            std::vector<D3D12CommandBuffer *> commands;
            uint64_t                          generation = 0;
            uint32_t                          issued     = 0;
        };
    } // namespace
    struct D3D12DeviceFrameContext::EncodingSession {
        EncodingTicket                         ticket;
        std::shared_ptr<D3D12CompiledFrame>    plan;
        TaskDispatcher                         dispatcher;
        std::future<DispatchState>             completion;
        std::vector<CommandBuffer *>           fragments;
        std::vector<std::optional<FrameError>> errors;
        std::atomic_bool                       cancelled{false};
    };
    struct D3D12DeviceFrameContext::State {
        struct Descriptors {
            ComPtr<ID3D12DescriptorHeap> heap;
            uint32_t                     cursor = 0;
        };
        FrameAllocationLimits                     limits;
        std::vector<std::array<Descriptors, 4>>   descriptors;
        std::vector<std::vector<WorkerPartition>> partitions;
        std::unique_ptr<ThreadPool>               pool;
        std::unique_ptr<EncodingSession>          session;
        uint32_t                                  maxRecordings = 0;
        const BackendCompiledFrame               *issuedPlan    = nullptr;
        CompiledFrameIdentity                     issuedIdentity;
        bool Encode(D3D12Device &device, EncodingSession &batch, uint32_t task, uint32_t worker, bool serial = false)
        {
            if (batch.cancelled.load(std::memory_order_acquire))
                return true;
            const auto &lease     = batch.ticket.frame.lease;
            auto       &partition = partitions[lease.slot][worker];
            auto        fail      = [&](GpuError error) {
                batch.errors[task] = FrameError{FrameErrorCode::ENCODE_FAILED, {}, error};
                return false;
            };
            // Only the actual worker resets its partition, after the previous lease retired.
            if (partition.generation != lease.generation) {
                const auto reset = partition.pool->TryReset();
                if (!reset.Succeeded())
                    return fail(reset.error);
                partition.generation = lease.generation;
                partition.issued     = 0;
            }
            auto      &encoder = *batch.plan->encoder;
            const auto ordinal = partition.issued;
            if (ordinal == partition.commands.size()) {
                auto allocation = partition.pool->AllocateChecked(encoder.requiresDepthResolve);
                if (const auto *error = std::get_if<GpuError>(&allocation))
                    return fail(*error);
                partition.commands.push_back(std::get<D3D12CommandBuffer *>(allocation));
            }
            ++partition.issued;
            auto      *command = partition.commands[ordinal];
            const auto begun   = command->BeginAfterPoolReset(encoder.requiresDepthResolve);
            if (FAILED(begun))
                return fail(D3D12Error(begun));
            if (serial)
                for (uint32_t i = 0; i < encoder.tasks.size(); ++i)
                    encoder.Encode(device.GetFunctions(), *command, command->GetNativeHandle(), i, command->GetNativeHandle1());
            else
                encoder.Encode(device.GetFunctions(), *command, command->GetNativeHandle(), task, command->GetNativeHandle1());
            const auto ended = command->EndChecked();
            if (encoder.encodeError && (SUCCEEDED(ended) || encoder.encodeError->code == FrameErrorCode::DEVICE_LOST ||
                                        encoder.encodeError->cause.code == GpuErrorCode::DEVICE_LOST)) {
                batch.errors[task] = *encoder.encodeError;
                return false;
            }
            if (FAILED(ended))
                return fail(D3D12Error(ended));
            batch.fragments[task] = command;
            return true;
        }
    };
    D3D12DeviceFrameContext::D3D12DeviceFrameContext(D3D12Device *device) : state(std::make_unique<State>()), device(device)
    {
    }
    D3D12DeviceFrameContext::~D3D12DeviceFrameContext()
    {
        if (state->session)
            state->session->cancelled.store(true, std::memory_order_release);
        if (state->pool)
            state->pool->WaitIdle();
        state->session.reset();
        // Worker command storage outlives every CPU borrower and submitted GPU use.
        if (!state->partitions.empty())
            device->GetQueue(QueueType::GRAPHICS)->WaitIdle();
    }
    bool D3D12DeviceFrameContext::InitBackend(const DeviceFrameContextInitInfo &info)
    {
        state->limits = info.allocations;
        if (info.allocations.samplerDescriptors > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE ||
            info.allocations.resourceDescriptors > D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1)
            return false;
        state->descriptors.resize(info.inflightNum);
        state->maxRecordings = info.maxRecordingsPerSlot;
        state->partitions.resize(info.inflightNum);
        for (auto &slot : state->partitions) {
            slot.resize(info.parallelNum);
            for (auto &partition : slot) {
                partition.pool = std::make_unique<D3D12CommandPool>(*device, D3D12_COMMAND_LIST_TYPE_DIRECT);
                if (!partition.pool->Init())
                    return false;
            }
        }
        if (info.parallelNum > 1)
            state->pool = std::make_unique<ThreadPool>(info.parallelNum, [](uint32_t index) { return new D3D12EncodeWorker(index); });
        return true;
    }
    FrameBufferStorageResult D3D12DeviceFrameContext::CreateFrameBuffer(const Buffer::Descriptor &desc)
    {
        CounterPtr<D3D12Buffer> buffer(new D3D12Buffer(*device));
        if (!buffer->Init(desc))
            return FrameError{FrameErrorCode::OUT_OF_MEMORY, {}, D3D12Error(buffer->GetCreateResult())};
        uint8_t *mapped = nullptr;
        if (desc.memory != MemoryType::GPU_ONLY) {
            mapped = buffer->Map();
            if (!mapped)
                return FrameError{FrameErrorCode::INVALID_STATE, {}, D3D12Error(buffer->GetMapResult())};
        }
        return FrameBufferStorage{std::move(buffer), mapped};
    }
    FrameOperationResult D3D12DeviceFrameContext::ResetLeaseResources(uint32_t slot) noexcept
    {
        for (auto &partition : state->descriptors[slot])
            partition.cursor = 0;
        return FrameOperationComplete{};
    }
    FrameBufferAllocation D3D12DeviceFrameContext::AllocateCopyScratch(const DeviceFrameLease &lease, uint64_t size)
    {
        return AllocateScratch(lease, size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
    }
    D3D12DescriptorAllocation
    D3D12DeviceFrameContext::AllocateDescriptors(const DeviceFrameLease &lease, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        if (type < D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type >= D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES || !count)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        const uint32_t capacities[] = {state->limits.resourceDescriptors, state->limits.samplerDescriptors, state->limits.renderTargets,
                                       state->limits.renderTargets};
        auto          &partition    = state->descriptors[lease.slot][type];
        if (count > capacities[type] - partition.cursor)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY};
        const bool visible = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        if (!partition.heap) {
            const D3D12_DESCRIPTOR_HEAP_DESC desc{type, capacities[type],
                                                  visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
            const auto status = device->GetFunctions().createDescriptorHeap(device->GetNativeHandle(), &desc, partition.heap.GetAddressOf());
            if (FAILED(status)) {
                FrameError error{FrameErrorCode::COMPILE_FAILED, {}, D3D12Error(status)};
                ObserveFailure(error);
                return error;
            }
        }
        D3D12FrameDescriptors result;
        result.heap       = partition.heap;
        const auto offset = uint64_t{partition.cursor} * device->GetNativeHandle()->GetDescriptorHandleIncrementSize(type);
        result.cpu        = partition.heap->GetCPUDescriptorHandleForHeapStart();
        result.cpu.ptr += static_cast<SIZE_T>(offset);
        if (visible) {
            result.gpu = partition.heap->GetGPUDescriptorHandleForHeapStart();
            result.gpu.ptr += offset;
        }
        partition.cursor += count;
        return result;
    }

    FrameCompileResult D3D12DeviceFrameContext::Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        if (!graph)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        const auto              scratchCursor = GetScratchCursor(lease);
        std::array<uint32_t, 4> cursors;
        for (uint32_t i = 0; i < cursors.size(); ++i)
            cursors[i] = state->descriptors[lease.slot][i].cursor;
        auto rollback = [&] {
            RestoreScratchCursor(lease, scratchCursor);
            for (uint32_t i = 0; i < cursors.size(); ++i)
                state->descriptors[lease.slot][i].cursor = cursors[i];
        };
        D3D12FrameCompiler compiler(*device, *this, lease);
        auto               result = compiler.Compile(std::move(graph));
        if (auto *error = std::get_if<FrameError>(&result)) {
            rollback();
            ObserveFailure(*error);
            return std::move(*error);
        }
        if (std::holds_alternative<FrameNoWork>(result)) {
            rollback();
            return FrameNoWork{};
        }
        auto plan     = std::move(std::get<std::unique_ptr<D3D12CompiledFrame>>(result));
        auto reserved = ReservePlan(lease, *plan->graph, plan->resources);
        if (auto *error = std::get_if<FrameError>(&reserved)) {
            rollback();
            return std::move(*error);
        }
        plan->reservation     = std::move(std::get<FramePlanLease>(reserved));
        state->issuedPlan     = plan.get();
        state->issuedIdentity = plan->GetIdentity();
        return std::unique_ptr<BackendCompiledFrame>(std::move(plan));
    }
    FrameRecordResult D3D12DeviceFrameContext::Finish(EncodingSession &batch)
    {
        FrameRecordingInputs inputs;
        inputs.commands = std::move(batch.fragments);
        inputs.payload  = std::make_shared<FrameGpuPayload<D3D12FrameEncoder>>(std::move(batch.plan->resources), std::move(batch.plan->encoder));
        inputs.receipt  = {std::move(batch.plan->graph), std::move(batch.plan->livePasses)};
        return FinishPlanRecording(batch.plan->reservation, std::move(inputs));
    }
    FrameDispatchResult D3D12DeviceFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &compiled)
    {
        if (!compiled || compiled.get() != state->issuedPlan || compiled->GetIdentity() != state->issuedIdentity)
            return FrameError{FrameErrorCode::WRONG_CONTEXT};
        auto *plan    = static_cast<D3D12CompiledFrame *>(compiled.get());
        auto  started = BeginEncoding(plan->reservation);
        if (std::holds_alternative<FrameError>(started))
            return started;
        auto batch    = std::make_unique<EncodingSession>();
        batch->ticket = std::get<EncodingTicket>(started);
        batch->plan.reset(static_cast<D3D12CompiledFrame *>(compiled.release()));
        state->issuedPlan   = nullptr;
        const auto   ticket = batch->ticket;
        const bool   serial = !state->pool || plan->coordinatorOnly;
        const size_t count  = serial ? 1 : plan->encoder->tasks.size();
        const auto   budget =
            state->pool ? std::min<uint64_t>(state->maxRecordings, uint64_t{state->pool->GetThreadCount()} * ThreadPool::GetLocalQueueCapacity())
                          : state->maxRecordings;
        if (count > budget) {
            PublishEncoding(ticket, FrameError{FrameErrorCode::OUT_OF_MEMORY, "DX12 encode task/recording budget exceeded"});
            return ticket;
        }
        batch->fragments.resize(count);
        batch->errors.resize(count);
        if (serial) {
            if (!state->Encode(*device, *batch, 0, 0, true))
                PublishEncoding(ticket, std::move(*batch->errors[0]));
            else
                PublishEncoding(ticket, Finish(*batch));
            return ticket;
        }
        for (uint32_t i = 0; i < count; ++i)
            batch->dispatcher.CreateCheckedTask([this, ptr = batch.get(), i](ThreadContext &thread) {
                return state->Encode(*device, *ptr, i, static_cast<D3D12EncodeWorker &>(thread).index);
            });
        auto dispatched = batch->dispatcher.Submit(*state->pool);
        if (!dispatched.Accepted()) {
            PublishEncoding(ticket, FrameError{FrameErrorCode::ENCODE_FAILED, "DX12 dispatcher rejected the batch"});
            return ticket;
        }
        batch->completion = std::move(dispatched.completion);
        state->session    = std::move(batch);
        return ticket;
    }
    FrameCollectResult D3D12DeviceFrameContext::TryCollect(const EncodingTicket &ticket)
    {
        if (state->session && state->session->ticket == ticket) {
            if (state->session->completion.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                return FramePending{};
            auto       session   = std::move(state->session);
            const auto completed = session->completion.get();
            if (auto *error = FindEncodingFailure(session->errors))
                PublishEncoding(ticket, std::move(*error));
            else if (completed != DispatchState::SUCCEEDED)
                PublishEncoding(ticket, FrameError{FrameErrorCode::ENCODE_FAILED});
            else if (session->cancelled.load(std::memory_order_acquire))
                PublishCancelledEncoding(ticket);
            else
                PublishEncoding(ticket, Finish(*session));
        }
        return BackendFrameContext::TryCollect(ticket);
    }
    FrameOperationResult D3D12DeviceFrameContext::Cancel(const EncodingTicket &ticket)
    {
        if (state->session && state->session->ticket == ticket) {
            state->session->cancelled.store(true, std::memory_order_release);
            return FrameOperationComplete{};
        }
        return BackendFrameContext::Cancel(ticket);
    }
    void D3D12DeviceFrameContext::SelectDirectSlot(uint32_t slot) noexcept
    {
        device->GetDescriptorAllocator()->BeginFrame(slot);
    }
} // namespace sky::aurora
