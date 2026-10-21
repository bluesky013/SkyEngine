
#include <common/FrameContext.h>
#include <common/FrameTransientPool.h>

#include <algorithm>
#include <array>
#include <aurora/rhi/Semaphore.h>
#include <core/environment/Singleton.h>
#include <cstring>
#include <map>
#include <mutex>

namespace sky::aurora {

    class FrameContextIds : public Singleton<FrameContextIds> {
    public:
        uint64_t Next()
        {
            std::lock_guard lock(mutex);
            return last == UINT64_MAX ? 0 : ++last;
        }

    private:
        std::mutex mutex;
        uint64_t   last = 0;
    };

    class FrameResourceTracking final {};

    struct FrameStateChanges {
        struct Change {
            const RefObject           *resource;
            FrameResourceBoundaryState final;
        };
        std::vector<Change> entries;
    };

    namespace {
        const RefObject *ResourceIdentity(const FrameResource &resource)
        {
            return std::visit([](const auto &value) -> const RefObject * { return value.Get(); }, resource);
        }
        bool IsDeviceLost(const FrameError &error) noexcept
        {
            return error.code == FrameErrorCode::DEVICE_LOST || error.cause.code == GpuErrorCode::DEVICE_LOST;
        }
        FrameError Error(FrameErrorCode code, GpuError cause = {}) noexcept
        {
            return {code, {}, cause};
        }
        class SlotRecording final : public RecordedFrame {
        public:
            const FrameGraphReceipt *GetGraphReceipt() const noexcept override
            {
                return &receipt;
            }
            CompiledFrameIdentity GetIdentity() const noexcept override
            {
                return identity;
            }
            CompiledFrameIdentity                    identity;
            FrameGraphReceipt                        receipt;
            SubmitInfo                               submission;
            std::vector<CounterPtr<RefObject>>       resources;
            std::shared_ptr<const void>              payload;
            std::shared_ptr<const void>              synchronizationOwner;
            std::shared_ptr<const uint8_t>           borrower;
            std::shared_ptr<const FrameStateChanges> states;
        };
        enum class SlotPhase { IDLE, RECORDING, SUBMITTED, QUARANTINED };
        enum class Protocol { UNSELECTED, EXPLICIT, DIRECT };
    } // namespace

    FrameError *FindEncodingFailure(std::span<std::optional<FrameError>> errors) noexcept
    {
        FrameError *first = nullptr;
        // A later device loss must close the context even when an earlier task failed recoverably.
        for (auto &error : errors) {
            if (!error)
                continue;
            if (IsDeviceLost(*error))
                return &*error;
            if (!first)
                first = &*error;
        }
        return first;
    }

    std::optional<uint64_t> FrameRangeOffset(uint64_t cursor, uint64_t capacity, uint64_t size, uint64_t alignment) noexcept
    {
        if (!size || !alignment || (alignment & (alignment - 1)) || cursor > capacity)
            return {};
        const auto padding = (alignment - (cursor & (alignment - 1))) & (alignment - 1);
        if (padding > capacity - cursor || size > capacity - cursor - padding)
            return {};
        return cursor + padding;
    }

    struct BackendFrameContext::State {
        struct Arena {
            FrameBufferStorage storage;
            uint64_t           cursor = 0;
        };
        struct Slot {
            std::array<Arena, 3>           arenas;
            std::unique_ptr<CommandPool>   pool;
            std::vector<CommandBuffer *>   commands;
            FencePtr                       fence;
            uint64_t                       generation = 0;
            uint32_t                       issued     = 0;
            SlotPhase                      phase      = SlotPhase::IDLE;
            SubmissionToken                token;
            std::weak_ptr<const uint8_t>   borrower;
            std::unique_ptr<RecordedFrame> retained;
            const RecordedFrame           *issuedRecording = nullptr;
            std::optional<EncodingTicket>  ticket;
            FrameCollectResult             encoded  = FramePending{};
            bool                           reported = false;
        };
        std::optional<FrameError> Validate(const DeviceFrameLease &lease) const noexcept
        {
            if (lease.context != identity)
                return Error(FrameErrorCode::WRONG_CONTEXT);
            if (lease.slot >= slots.size() || lease.generation == 0 || slots[lease.slot].generation != lease.generation)
                return Error(FrameErrorCode::STALE_LEASE);
            if (protocol != Protocol::EXPLICIT || active != lease.slot || slots[lease.slot].phase != SlotPhase::RECORDING)
                return Error(FrameErrorCode::INVALID_STATE);
            return {};
        }
        struct TrackedResource {
            FrameResource                              pin;
            FrameResourceStateSnapshot                 state;
            uint32_t                                   pending           = 0;
            uint32_t                                   imageSubresources = 0;
            bool                                       owned             = false;
            std::weak_ptr<const FrameResourceTracking> owner;
        };
        bool CanChangeTracking() const noexcept
        {
            if (terminal || protocol == Protocol::DIRECT)
                return false;
            if (active == UINT32_MAX)
                return true;
            const auto &slot = slots[active];
            return !slot.issued && !slot.ticket && slot.borrower.expired();
        }
        void RetireOwnedResources()
        {
            if (!CanChangeTracking())
                return;
            std::erase_if(tracked, [&](const auto &entry) {
                const auto &resource = entry.second;
                if (!resource.owned || !resource.owner.expired() || resource.pending)
                    return false;
                trackedImageSubresources -= resource.imageSubresources;
                return true;
            });
        }
        std::map<const RefObject *, TrackedResource> tracked;
        Device                                      *device               = nullptr;
        uint32_t                                     maxTracked           = 0;
        uint32_t                                     maxImageSubresources = 0, trackedImageSubresources = 0;
        FrameAllocationLimits                        limits;
        uint64_t                                     uniformAlignment = 1;
        Queue                                       *queue            = nullptr;
        FrameContextIdentity                         identity;
        std::vector<Slot>                            slots;
        mutable Protocol                             protocol      = Protocol::UNSELECTED;
        uint32_t                                     active        = UINT32_MAX;
        uint32_t                                     cursor        = 0;
        uint32_t                                     directSlot    = 0;
        uint64_t                                     serial        = 0;
        uint64_t                                     recording     = 0;
        uint64_t                                     session       = 0;
        uint32_t                                     maxRecordings = 0;
        bool                                         terminal      = false;
    };

    BackendFrameContext::BackendFrameContext() : state(std::make_unique<State>())
    {
    }
    BackendFrameContext::~BackendFrameContext()
    {
        // Quarantined resources never retire merely because an error was reported.
        if (state->queue && std::any_of(state->slots.begin(), state->slots.end(), [](const auto &slot) { return bool(slot.retained); }))
            state->queue->WaitIdle();
    }
    bool BackendFrameContext::Init(Device *device, const DeviceFrameContextInitInfo &info)
    {
        if (!device || !info.inflightNum || !info.parallelNum || !info.deviceEpoch || !info.maxRecordingsPerSlot)
            return false;
        if (info.vulkanCompiler > VulkanFrameGraphMode::RENDER_PASS ||
            (device->GetAPI() != API::VULKAN && info.vulkanCompiler != VulkanFrameGraphMode::DYNAMIC_RENDERING))
            return false;
        state->device               = device;
        state->maxTracked           = info.trackedResources;
        state->maxImageSubresources = info.trackedImageSubresources;
        state->limits               = info.allocations;
        state->uniformAlignment     = std::max(1u, device->GetCapability().minUniformBufferOffsetAlignment);
        if ((state->uniformAlignment & (state->uniformAlignment - 1)) || info.allocations.stagingBytes > SIZE_MAX ||
            info.allocations.uniformBytes > SIZE_MAX)
            return false;
        state->maxRecordings = info.maxRecordingsPerSlot;
        state->queue         = device->GetQueue(QueueType::GRAPHICS);
        state->identity      = {device->GetAPI(), info.deviceEpoch, FrameContextIds::Get()->Next()};
        if (!state->queue || !state->identity.contextId)
            return false;
        state->slots.resize(info.inflightNum);
        for (auto &slot : state->slots) {
            slot.fence = device->CreateFence({true});
            if (!slot.fence)
                return false;
            slot.pool.reset(device->CreateCommandPool(QueueType::GRAPHICS));
            if (!slot.pool)
                return false;
            auto *command = slot.pool->Allocate();
            if (!command)
                return false;
            slot.commands.push_back(command);
        }
        return InitBackend(info);
    }
    FrameContextIdentity BackendFrameContext::GetIdentity() const noexcept
    {
        return state->identity;
    }

    FrameOperationResult BackendFrameContext::TrackResource(FrameResource resource, FrameResourceBoundaryState initial)
    {
        if (!state->CanChangeTracking())
            return Error(FrameErrorCode::INVALID_STATE);
        state->RetireOwnedResources();
        const auto *key = ResourceIdentity(resource);
        if (!key || resource.index() != initial.index() ||
            !std::visit([&](const auto &value) { return value->GetDevice() == state->device; }, resource))
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        uint32_t imageSubresources = 0;
        if (const auto *bufferState = std::get_if<CopyBufferState>(&initial)) {
            if (!IsValidFrameBufferState(*bufferState))
                return Error(FrameErrorCode::INVALID_ARGUMENT);
        } else {
            const auto count = std::get<ImagePtr>(resource)->GetMipLayerCount();
            if (auto error = ValidateImageBoundaryState(std::get<ImageBoundaryState>(initial), count))
                return *error;
            imageSubresources = static_cast<uint32_t>(count);
        }
        if (state->tracked.contains(key))
            return Error(FrameErrorCode::INVALID_STATE);
        if (state->tracked.size() >= state->maxTracked || imageSubresources > state->maxImageSubresources - state->trackedImageSubresources)
            return Error(FrameErrorCode::OUT_OF_MEMORY);
        state->tracked.emplace(key, State::TrackedResource{std::move(resource), {{initial, {}}, {initial, {}}}, 0, imageSubresources});
        state->trackedImageSubresources += imageSubresources;
        state->protocol = Protocol::EXPLICIT;
        return FrameOperationComplete{};
    }
    FrameResourceTrackingResult BackendFrameContext::TrackOwnedResource(FrameResource resource, FrameResourceBoundaryState initial)
    {
        const auto *key    = ResourceIdentity(resource);
        auto        result = TrackResource(std::move(resource), initial);
        if (const auto *error = std::get_if<FrameError>(&result))
            return *error;
        auto  owner   = std::make_shared<const FrameResourceTracking>();
        auto &tracked = state->tracked.at(key);
        tracked.owned = true;
        tracked.owner = owner;
        return owner;
    }
    FrameResourceStateResult BackendFrameContext::GetResourceState(const FrameResource &resource) const
    {
        if (state->terminal)
            return Error(FrameErrorCode::INVALID_STATE);
        const auto found = state->tracked.find(ResourceIdentity(resource));
        if (found == state->tracked.end())
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        return found->second.state;
    }
    FrameOperationResult BackendFrameContext::UntrackResource(const FrameResource &resource)
    {
        if (!state->CanChangeTracking())
            return Error(FrameErrorCode::INVALID_STATE);
        const auto found = state->tracked.find(ResourceIdentity(resource));
        if (found == state->tracked.end())
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        const auto &snapshot = found->second.state;
        if (found->second.owned || found->second.pending || snapshot.accepted.submission != snapshot.completed.submission)
            return Error(FrameErrorCode::INVALID_STATE);
        state->trackedImageSubresources -= found->second.imageSubresources;
        state->tracked.erase(found);
        return FrameOperationComplete{};
    }

    BeginSlotResult BackendFrameContext::TryBeginSlot() noexcept
    {
        if (state->protocol == Protocol::DIRECT || state->terminal)
            return Error(FrameErrorCode::INVALID_STATE);
        state->protocol = Protocol::EXPLICIT;
        state->RetireOwnedResources();
        if (state->active != UINT32_MAX)
            return FrameBusy{};
        for (uint32_t offset = 0; offset < state->slots.size(); ++offset) {
            const auto index = (state->cursor + offset) % state->slots.size();
            auto      &slot  = state->slots[index];
            if (slot.phase != SlotPhase::IDLE || slot.generation == UINT64_MAX)
                continue;
            const auto reset = slot.pool->TryReset();
            if (!reset.Succeeded()) {
                state->terminal = true;
                return Error(reset.error.code == GpuErrorCode::DEVICE_LOST ? FrameErrorCode::DEVICE_LOST : FrameErrorCode::INVALID_STATE,
                             reset.error);
            }
            auto resources = ResetLeaseResources(static_cast<uint32_t>(index));
            if (auto *error = std::get_if<FrameError>(&resources)) {
                ObserveFailure(*error);
                return std::move(*error);
            }
            for (auto &arena : slot.arenas)
                arena.cursor = 0;
            ++slot.generation;
            slot.issuedRecording = nullptr;
            slot.phase           = SlotPhase::RECORDING;
            slot.issued          = 0;
            slot.reported        = false;
            state->active        = static_cast<uint32_t>(index);
            state->cursor        = (state->active + 1) % static_cast<uint32_t>(state->slots.size());
            return DeviceFrameLease{state->identity, state->active, slot.generation};
        }
        if (std::all_of(state->slots.begin(), state->slots.end(), [](const auto &slot) { return slot.generation == UINT64_MAX; }))
            return Error(FrameErrorCode::INVALID_STATE);
        return FrameBusy{};
    }

    std::variant<FrameError, uint64_t>
    BackendFrameContext::AvailableFrameBufferBytes(const DeviceFrameLease &lease, FrameBufferKind kind, uint64_t alignment) const
    {
        const auto valid = ValidateCompileLease(lease);
        if (const auto *error = std::get_if<FrameError>(&valid))
            return *error;
        if (kind > FrameBufferKind::UNIFORM || !alignment || (alignment & (alignment - 1)))
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        const bool uniform  = kind == FrameBufferKind::UNIFORM;
        const auto capacity = uniform ? state->limits.uniformBytes : state->limits.stagingBytes;
        if (uniform)
            alignment = std::max(alignment, state->uniformAlignment);
        const auto offset    = FrameRangeOffset(state->slots[lease.slot].arenas[static_cast<uint32_t>(kind)].cursor, capacity, 1, alignment);
        const auto remaining = offset ? capacity - *offset : 0;
        return uniform ? remaining - remaining % state->uniformAlignment : remaining;
    }
    FrameBufferAllocation BackendFrameContext::AllocateBuffer(const DeviceFrameLease &lease, uint32_t kind, uint64_t size, uint64_t alignment)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        if (!size || !alignment || (alignment & (alignment - 1)))
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        const uint64_t capacities[] = {state->limits.stagingBytes, state->limits.uniformBytes, state->limits.scratchBytes};
        auto          &arena        = state->slots[lease.slot].arenas[kind];
        if (kind == 1) {
            alignment         = std::max(alignment, state->uniformAlignment);
            const auto padded = FrameRangeOffset(size, UINT64_MAX, 1, state->uniformAlignment);
            if (!padded)
                return Error(FrameErrorCode::OUT_OF_MEMORY);
            size = *padded;
        }
        const auto offset = FrameRangeOffset(arena.cursor, capacities[kind], size, alignment);
        if (!offset)
            return Error(FrameErrorCode::OUT_OF_MEMORY);
        if (!arena.storage.buffer) {
            Buffer::Descriptor desc;
            desc.size    = capacities[kind];
            desc.memory  = kind == 2 ? MemoryType::GPU_ONLY : MemoryType::CPU_TO_GPU;
            desc.usage   = kind == 1 ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST;
            auto created = CreateFrameBuffer(desc);
            if (auto *error = std::get_if<FrameError>(&created)) {
                ObserveFailure(*error);
                return std::move(*error);
            }
            arena.storage = std::move(std::get<FrameBufferStorage>(created));
        }
        arena.cursor = *offset + size;
        return FrameBufferSlice{lease, arena.storage.buffer, *offset, size};
    }
    FrameBufferAllocation
    BackendFrameContext::WriteFrameBuffer(const DeviceFrameLease &lease, FrameBufferKind kind, std::span<const std::byte> bytes, uint64_t alignment)
    {
        if (kind > FrameBufferKind::UNIFORM || bytes.empty())
            return Error(FrameErrorCode::INVALID_ARGUMENT);
        auto result = AllocateBuffer(lease, static_cast<uint32_t>(kind), bytes.size(), alignment);
        if (auto *slice = std::get_if<FrameBufferSlice>(&result)) {
            auto *destination = state->slots[lease.slot].arenas[static_cast<uint32_t>(kind)].storage.mapped + slice->offset;
            std::memcpy(destination, bytes.data(), bytes.size());
            std::memset(destination + bytes.size(), 0, static_cast<size_t>(slice->size - bytes.size()));
        }
        return result;
    }
    FrameBufferAllocation BackendFrameContext::AllocateScratch(const DeviceFrameLease &lease, uint64_t size, uint64_t alignment)
    {
        return AllocateBuffer(lease, 2, size, alignment);
    }
    uint64_t BackendFrameContext::GetScratchCursor(const DeviceFrameLease &lease) const noexcept
    {
        return state->slots[lease.slot].arenas[2].cursor;
    }
    void BackendFrameContext::RestoreScratchCursor(const DeviceFrameLease &lease, uint64_t cursor) noexcept
    {
        state->slots[lease.slot].arenas[2].cursor = cursor;
    }

    CommandAllocationResult BackendFrameContext::AllocateCommand(const DeviceFrameLease &lease, uint32_t ordinal) noexcept
    {
        if (auto error = state->Validate(lease))
            return std::move(*error);
        if (!state->tracked.empty() || !state->slots[lease.slot].borrower.expired() || state->slots[lease.slot].ticket)
            return Error(FrameErrorCode::INVALID_STATE);
        return Allocate(lease, ordinal);
    }
    CommandAllocationResult BackendFrameContext::Allocate(const DeviceFrameLease &lease, uint32_t ordinal) noexcept
    {
        auto &slot = state->slots[lease.slot];
        if (ordinal != slot.issued || ordinal >= state->maxRecordings)
            return Error(FrameErrorCode::INVALID_STATE);

        if (ordinal == slot.commands.size()) {
            slot.commands.reserve(slot.commands.size() + 1);
            auto *command = slot.pool->Allocate();
            if (!command)
                return Error(FrameErrorCode::OUT_OF_MEMORY);
            slot.commands.push_back(command);
        }
        ++slot.issued;
        return slot.commands[ordinal];
    }

    FrameRecordResult BackendFrameContext::FinishRecording(const DeviceFrameLease &lease, FrameRecordingInputs inputs) noexcept
    {
        return Record(lease, std::move(inputs), nullptr);
    }
    FrameRecordResult BackendFrameContext::Record(const DeviceFrameLease &lease, FrameRecordingInputs inputs, const FramePlanLease *plan) noexcept
    {
        if (auto error = state->Validate(lease))
            return std::move(*error);
        auto &slot = state->slots[lease.slot];
        if ((!slot.issued && inputs.commands.empty()) ||
            (!inputs.commands.empty() &&
             (!plan || slot.issued || inputs.commands.size() > state->maxRecordings ||
              std::any_of(inputs.commands.begin(), inputs.commands.end(), [](const auto *command) { return !command; }))) ||
            (!plan && (!slot.borrower.expired() || slot.ticket || state->recording == UINT64_MAX)))
            return Error(FrameErrorCode::INVALID_STATE);

        auto frame      = std::make_unique<SlotRecording>();
        frame->identity = plan ? plan->identity : CompiledFrameIdentity{lease, 0, state->recording + 1};
        frame->borrower = plan ? plan->borrower : std::make_shared<const uint8_t>(0);
        frame->states   = plan ? plan->states : nullptr;
        if (inputs.commands.empty())
            frame->submission.commandBuffers.assign(slot.commands.begin(), slot.commands.begin() + slot.issued);
        else
            frame->submission.commandBuffers = std::move(inputs.commands);
        frame->submission.waitSemaphores   = std::move(inputs.waits);
        frame->submission.signalSemaphores = std::move(inputs.signals);
        frame->resources                   = std::move(inputs.resources);
        frame->payload                     = std::move(inputs.payload);
        frame->receipt                     = std::move(inputs.receipt);
        for (const auto &wait : frame->submission.waitSemaphores)
            if (wait.semaphore)
                frame->resources.emplace_back(wait.semaphore);
        for (const auto &signal : frame->submission.signalSemaphores)
            if (signal.semaphore)
                frame->resources.emplace_back(signal.semaphore);
        slot.borrower        = frame->borrower;
        slot.issuedRecording = frame.get();
        if (!plan)
            ++state->recording;
        return std::unique_ptr<RecordedFrame>(std::move(frame));
    }

    FrameSubmitResult BackendFrameContext::Submit(std::unique_ptr<RecordedFrame> &recorded) noexcept
    {
        return Submit(recorded, {});
    }
    FrameSubmitResult BackendFrameContext::Submit(std::unique_ptr<RecordedFrame> &recorded, const FrameSubmissionSync &sync) noexcept
    {
        if (!recorded)
            return SubmissionRejected{Error(FrameErrorCode::INVALID_ARGUMENT)};
        if (state->terminal)
            return SubmissionRejected{Error(FrameErrorCode::INVALID_STATE)};
        const auto identity = recorded->GetIdentity();
        if (auto error = state->Validate(identity.lease))
            return SubmissionRejected{std::move(*error)};
        auto &slot = state->slots[identity.lease.slot];
        if (recorded.get() != slot.issuedRecording || slot.borrower.expired() || state->serial == UINT64_MAX)
            return SubmissionRejected{Error(FrameErrorCode::INVALID_STATE)};
        auto *frame = static_cast<SlotRecording *>(recorded.get());
        if (slot.borrower.lock() != frame->borrower)
            return SubmissionRejected{Error(FrameErrorCode::INVALID_STATE)};
        if ((!sync.waits.empty() || !sync.signals.empty()) && !sync.owner)
            return SubmissionRejected{Error(FrameErrorCode::INVALID_ARGUMENT)};
        if (!sync.owner && frame->receipt.graph &&
            std::any_of(frame->receipt.graph->GetData().images.begin(), frame->receipt.graph->GetData().images.end(),
                        [](const auto &image) { return image.image.IsBorrowed(); }))
            return SubmissionRejected{Error(FrameErrorCode::INVALID_ARGUMENT)};
        for (const auto *semaphores : {&sync.waits, &sync.signals})
            for (const auto &entry : *semaphores)
                if (!entry.semaphore || entry.semaphore->GetDevice() != state->device || !entry.stageMask)
                    return SubmissionRejected{Error(FrameErrorCode::INVALID_ARGUMENT)};
        SubmitInfo submission = frame->submission;
        submission.waitSemaphores.insert(submission.waitSemaphores.end(), sync.waits.begin(), sync.waits.end());
        submission.signalSemaphores.insert(submission.signalSemaphores.end(), sync.signals.begin(), sync.signals.end());
        const auto reset = slot.fence->TryReset();
        if (!reset.Succeeded()) {
            state->terminal = reset.error.code == GpuErrorCode::DEVICE_LOST;
            return SubmissionRejected{Error(state->terminal ? FrameErrorCode::DEVICE_LOST : FrameErrorCode::SUBMIT_REJECTED, reset.error)};
        }
        submission.fence  = slot.fence.Get();
        const auto result = state->queue->TrySubmit(submission);
        if (result.status == QueueSubmitStatus::REJECTED) {
            state->terminal = result.error.code == GpuErrorCode::DEVICE_LOST;
            return SubmissionRejected{Error(FrameErrorCode::SUBMIT_REJECTED, result.error)};
        }
        frame->synchronizationOwner = sync.owner;
        slot.token                  = {identity.lease, ++state->serial};
        slot.retained               = std::move(recorded);
        frame->receipt              = {};
        state->active               = UINT32_MAX;
        if (result.status == QueueSubmitStatus::INDETERMINATE) {
            slot.phase      = SlotPhase::QUARANTINED;
            state->terminal = true;
            return SubmissionIndeterminate{slot.token, Error(FrameErrorCode::SUBMIT_UNCERTAIN, result.error)};
        }
        if (frame->states)
            for (const auto &change : frame->states->entries) {
                auto &tracked          = state->tracked.at(change.resource);
                tracked.state.accepted = {change.final, slot.token};
                ++tracked.pending;
            }
        slot.phase = SlotPhase::SUBMITTED;
        return SubmissionAccepted{slot.token};
    }

    FrameOperationResult BackendFrameContext::DiscardUnsubmitted(const DeviceFrameLease &lease) noexcept
    {
        if (auto error = state->Validate(lease))
            return std::move(*error);
        auto &slot = state->slots[lease.slot];
        if (!slot.borrower.expired() || slot.ticket)
            return Error(FrameErrorCode::INVALID_STATE);
        slot.phase    = SlotPhase::IDLE;
        state->active = UINT32_MAX;
        state->RetireOwnedResources();
        return FrameOperationComplete{};
    }

    size_t BackendFrameContext::PollCompleted(std::span<FrameCompletion> destination) noexcept
    {
        size_t written = 0;
        for (auto &slot : state->slots) {
            if (written == destination.size())
                break;
            if (!slot.retained || slot.reported)
                continue;
            const auto query = slot.fence->Query();
            if (query.status == FenceStatus::PENDING)
                continue;
            auto &completion = destination[written++];
            completion.token = slot.token;
            completion.error = {};
            slot.reported    = true;
            if (query.status == FenceStatus::SIGNALED) {
                completion.status = FrameCompletionStatus::COMPLETE;
                const auto &frame = static_cast<const SlotRecording &>(*slot.retained);
                if (!state->terminal && frame.states)
                    for (const auto &change : frame.states->entries) {
                        auto &tracked   = state->tracked.at(change.resource);
                        auto &completed = tracked.state.completed;
                        --tracked.pending;
                        if (slot.token.serial > completed.submission.serial)
                            completed = {change.final, slot.token};
                    }
                slot.retained.reset();
                slot.phase = SlotPhase::IDLE;
            } else {
                completion.status =
                    query.error.code == GpuErrorCode::DEVICE_LOST ? FrameCompletionStatus::DEVICE_LOST : FrameCompletionStatus::FAILED;
                completion.error =
                    Error(query.error.code == GpuErrorCode::DEVICE_LOST ? FrameErrorCode::DEVICE_LOST : FrameErrorCode::INVALID_STATE, query.error);
                slot.phase      = SlotPhase::QUARANTINED;
                state->terminal = true;
            }
        }
        state->RetireOwnedResources();
        return written;
    }

    FrameCompileResult BackendFrameContext::Compile(const DeviceFrameLease &, std::shared_ptr<const FrozenFrameGraph>)
    {
        return Error(FrameErrorCode::UNSUPPORTED);
    }
    FrameDispatchResult BackendFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &)
    {
        return Error(FrameErrorCode::UNSUPPORTED);
    }
    FrameOperationResult BackendFrameContext::ValidateCompileLease(const DeviceFrameLease &lease) const noexcept
    {
        if (auto error = state->Validate(lease))
            return std::move(*error);
        const auto &slot = state->slots[lease.slot];
        if (state->terminal || slot.issued || slot.ticket || !slot.borrower.expired())
            return Error(FrameErrorCode::INVALID_STATE);
        return FrameOperationComplete{};
    }
    std::variant<FrameError, FramePlanLease>
    BackendFrameContext::ReservePlan(const DeviceFrameLease &lease, const FrozenFrameGraph &graph, const FrameResourceBindings &bindings) noexcept
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        for (const auto &image : bindings.images)
            if (image && !image.ValidFor(lease))
                return Error(FrameErrorCode::STALE_LEASE);
        if (!graph.GetId() || state->recording == UINT64_MAX)
            return Error(FrameErrorCode::INVALID_STATE);
        auto &slot = state->slots[lease.slot];

        std::shared_ptr<FrameStateChanges> changes;
        if (!state->tracked.empty()) {
            changes            = std::make_shared<FrameStateChanges>();
            const auto &data   = graph.GetData();
            auto        append = [&](const RefObject *key, FrameResourceBoundaryState initial, FrameResourceBoundaryState final) {
                const auto found = state->tracked.find(key);
                if (found == state->tracked.end())
                    return true;
                if (found->second.state.accepted.state != initial)
                    return false;
                changes->entries.push_back({key, final});
                return true;
            };
            for (size_t i = 0; i < bindings.buffers.size(); ++i)
                if (bindings.buffers[i] && !data.buffers[i].transient &&
                    !append(bindings.buffers[i].Get(), data.buffers[i].initial, data.buffers[i].final))
                    return FrameError{FrameErrorCode::INVALID_STATE, "Buffer initial state differs from the accepted queue state"};
            for (size_t i = 0; i < bindings.images.size(); ++i)
                if (bindings.images[i] && !data.images[i].transient &&
                    !append(bindings.images[i].Get(), data.images[i].initial, data.images[i].final))
                    return FrameError{FrameErrorCode::INVALID_STATE, "Image initial state differs from the accepted queue state"};
        }
        FramePlanLease plan{{lease, graph.GetId(), state->recording + 1}, std::make_shared<const uint8_t>(0), std::move(changes)};
        slot.issuedRecording = nullptr;
        slot.borrower        = plan.borrower;
        ++state->recording;
        return plan;
    }
    FrameOperationResult BackendFrameContext::ValidatePlan(const FramePlanLease &plan) const noexcept
    {
        if (auto error = state->Validate(plan.identity.lease))
            return std::move(*error);
        if (state->terminal || !plan.borrower || state->slots[plan.identity.lease.slot].borrower.lock() != plan.borrower)
            return Error(FrameErrorCode::INVALID_STATE);
        return FrameOperationComplete{};
    }
    FrameDispatchResult BackendFrameContext::BeginEncoding(const FramePlanLease &plan) noexcept
    {
        auto valid = ValidatePlan(plan);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        auto &slot = state->slots[plan.identity.lease.slot];
        if (slot.ticket || slot.issued || state->session == UINT64_MAX)
            return Error(FrameErrorCode::INVALID_STATE);
        EncodingTicket ticket{plan.identity, ++state->session};
        slot.ticket  = ticket;
        slot.encoded = FramePending{};
        return ticket;
    }
    CommandAllocationResult BackendFrameContext::AllocatePlanCommand(const FramePlanLease &plan, uint32_t ordinal) noexcept
    {
        auto valid = ValidatePlan(plan);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        return Allocate(plan.identity.lease, ordinal);
    }
    FrameRecordResult BackendFrameContext::FinishPlanRecording(const FramePlanLease &plan, FrameRecordingInputs inputs) noexcept
    {
        auto valid = ValidatePlan(plan);
        if (auto *error = std::get_if<FrameError>(&valid))
            return std::move(*error);
        return Record(plan.identity.lease, std::move(inputs), &plan);
    }
    void BackendFrameContext::ObserveFailure(const FrameError &error) noexcept
    {
        if (IsDeviceLost(error))
            state->terminal = true;
    }
    void BackendFrameContext::PublishEncoding(const EncodingTicket &ticket, FrameRecordResult result) noexcept
    {
        auto &slot = state->slots[ticket.frame.lease.slot];
        if (auto *error = std::get_if<FrameError>(&result)) {
            ObserveFailure(*error);
            slot.encoded = EncodingFailed{std::move(*error)};
        } else
            slot.encoded = std::move(std::get<std::unique_ptr<RecordedFrame>>(result));
    }
    void BackendFrameContext::PublishCancelledEncoding(const EncodingTicket &ticket) noexcept
    {
        state->slots[ticket.frame.lease.slot].encoded = EncodingCancelled{};
    }
    FrameCollectResult BackendFrameContext::TryCollect(const EncodingTicket &ticket)
    {
        if (auto error = state->Validate(ticket.frame.lease))
            return std::move(*error);
        auto &slot = state->slots[ticket.frame.lease.slot];
        if (!slot.ticket || *slot.ticket != ticket)
            return Error(FrameErrorCode::INVALID_STATE);
        if (std::holds_alternative<FramePending>(slot.encoded))
            return FramePending{};
        auto result  = std::move(slot.encoded);
        slot.encoded = FramePending{};
        slot.ticket.reset();
        return result;
    }
    FrameOperationResult BackendFrameContext::Cancel(const EncodingTicket &ticket)
    {
        if (auto error = state->Validate(ticket.frame.lease))
            return std::move(*error);
        auto &slot = state->slots[ticket.frame.lease.slot];
        if (!slot.ticket || *slot.ticket != ticket || std::holds_alternative<FramePending>(slot.encoded))
            return Error(FrameErrorCode::INVALID_STATE);
        if (!std::holds_alternative<EncodingFailed>(slot.encoded))
            slot.encoded = EncodingCancelled{};
        return FrameOperationComplete{};
    }

    Fence *BackendFrameContext::GetFrameFence() const noexcept
    {
        if (state->protocol == Protocol::EXPLICIT || state->slots.empty())
            return nullptr;
        state->protocol = Protocol::DIRECT;
        return state->slots[state->directSlot].fence.Get();
    }
    void BackendFrameContext::BeginFrame() noexcept
    {
        if (auto *fence = GetFrameFence()) {
            fence->Wait();
            fence->Reset();
            SelectDirectSlot(state->directSlot);
        }
    }
    void BackendFrameContext::EndFrame() noexcept
    {
        if (state->protocol == Protocol::DIRECT)
            state->directSlot = (state->directSlot + 1) % static_cast<uint32_t>(state->slots.size());
    }

} // namespace sky::aurora
