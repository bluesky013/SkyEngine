//
// Created on 2026/10/17.
//

#include "frame/FrameScheduler.h"
#include "resource/ResourceUploader.h"

#include <algorithm>
#include <aurora/rhi/Device.h>

namespace sky::aurora {
    struct FrameScheduler::State {
        enum class Phase : uint8_t { ACQUIRED, ABANDONED, SUBMITTED, PRESENTED, QUARANTINED };
        struct Frame {
            AcquiredSwapChainFrame      native;
            DeviceFrameLease            lease;
            std::weak_ptr<const void>   borrower;
            SubmissionToken             token;
            Phase                       phase       = Phase::ACQUIRED;
            bool                        gpuComplete = false;
            std::shared_ptr<const void> retained;
        };
        struct Output {
            OutputSnapshot     snapshot;
            SwapChainPtr       swapchain;
            std::vector<Frame> frames;
            bool               closing       = false;
            bool               failed        = false;
            bool               resizePending = false;
            PresentationStatus surfaceStatus = PresentationStatus::READY;
        };
        Device             &device;
        DeviceFrameContext &context;
        std::vector<Output> outputs;
        bool                stopping = false;
        GpuError            deviceError;
        Output             *Find(ViewportId id)
        {
            const auto found = std::find_if(outputs.begin(), outputs.end(), [id](const auto &entry) { return entry.snapshot.viewport == id; });
            return found == outputs.end() ? nullptr : &*found;
        }
        SurfaceQueryResult Refresh(Output &output)
        {
            const auto  queried       = output.swapchain->QuerySurface();
            const auto &configuration = queried.configuration;
            const bool  valid         = queried.status == PresentationStatus::READY || queried.status == PresentationStatus::OUT_OF_DATE;
            const bool  changed =
                valid && (output.snapshot.extent.width != configuration.extent.width ||
                          output.snapshot.extent.height != configuration.extent.height || output.snapshot.transform != configuration.transform ||
                          output.snapshot.surfaceRevision != configuration.revision || output.snapshot.format != configuration.format);
            if (changed || output.surfaceStatus != queried.status) {
                if (output.snapshot.revision == UINT64_MAX) {
                    output.failed = true;
                    return {{PresentationStatus::FAILED, {GpuErrorCode::INVALID_ARGUMENT}}, {}};
                }
                ++output.snapshot.revision;
            }
            if (valid) {
                output.snapshot.extent          = configuration.extent;
                output.snapshot.transform       = configuration.transform;
                output.snapshot.surfaceRevision = configuration.revision;
                output.snapshot.format          = configuration.format;
            }
            output.surfaceStatus = queried.status;
            if (queried.status == PresentationStatus::OUT_OF_DATE)
                output.resizePending = true;
            if (queried.status == PresentationStatus::SURFACE_LOST)
                output.failed = true;
            if (queried.error.code == GpuErrorCode::DEVICE_LOST)
                deviceError = queried.error;
            return queried;
        }
    };

    FrameScheduler::FrameScheduler(Device &device, DeviceFrameContext &context) : state(std::make_unique<State>(device, context))
    {
    }
    FrameScheduler::~FrameScheduler() = default;

    FrameOperationResult FrameScheduler::AddOutput(ViewportId id, SwapChainPtr swapchain)
    {
        if (state->deviceError.code != GpuErrorCode::NONE)
            return FrameError{FrameErrorCode::DEVICE_LOST, {}, state->deviceError};
        if (state->stopping || !id.IsValid() || !swapchain || swapchain->GetDevice() != &state->device || state->outputs.size() >= 64)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        if (std::any_of(state->outputs.begin(), state->outputs.end(),
                        [&](const auto &entry) { return entry.snapshot.viewport.index == id.index || entry.swapchain.Get() == swapchain.Get(); }))
            return FrameError{FrameErrorCode::INVALID_STATE};
        const auto extent = swapchain->GetExtent();
        state->outputs.push_back({{id, 1, extent}, std::move(swapchain)});
        const auto queried = state->Refresh(state->outputs.back());
        if (queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST) {
            state->outputs.pop_back();
            return FrameError{queried.error.code == GpuErrorCode::DEVICE_LOST ? FrameErrorCode::DEVICE_LOST : FrameErrorCode::INVALID_STATE,
                              "Output surface query failed", queried.error};
        }
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameScheduler::RequestResize(ViewportId id, Extent2D extent)
    {
        auto *output = state->Find(id);
        if (!output || output->closing || output->snapshot.revision == UINT64_MAX)
            return FrameError{FrameErrorCode::INVALID_STATE};
        if (output->snapshot.extent.width != extent.width || output->snapshot.extent.height != extent.height) {
            output->snapshot.extent = extent;
            ++output->snapshot.revision;
            output->resizePending = true;
        }
        return FrameOperationComplete{};
    }
    FrameOperationResult FrameScheduler::RequestClose(ViewportId id)
    {
        auto *output = state->Find(id);
        if (!output)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        output->closing = true;
        return FrameOperationComplete{};
    }
    void FrameScheduler::RequestShutdown()
    {
        state->stopping = true;
        for (auto &output : state->outputs)
            output.closing = true;
    }
    std::variant<OutputSnapshot, OutputProgress> FrameScheduler::BeginViewport(ViewportId id)
    {
        auto *output = state->Find(id);
        if (!output)
            return OutputProgress{id, OutputProgressKind::FAILED, {GpuErrorCode::INVALID_ARGUMENT}};
        if (state->deviceError.code != GpuErrorCode::NONE)
            return OutputProgress{id, OutputProgressKind::FAILED, state->deviceError};
        if (state->stopping || output->closing || output->failed)
            return OutputProgress{id};
        const auto queried = state->Refresh(*output);
        if (queried.status == PresentationStatus::READY && !output->resizePending)
            return output->snapshot;
        return OutputProgress{
            id,
            queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST ? OutputProgressKind::FAILED
                                                                                                               : OutputProgressKind::SKIPPED,
            queried.error, queried.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE : OutputFailureDisposition::RETRY};
    }
    std::vector<OutputSnapshot> FrameScheduler::Snapshot() const
    {
        std::vector<OutputSnapshot> result;
        for (const auto &output : state->outputs)
            if (!output.closing && !output.failed)
                result.push_back(output.snapshot);
        return result;
    }
    std::variant<FrameError, OutputAcquireBatch> FrameScheduler::Acquire(const DeviceFrameLease &lease, std::span<const OutputSnapshot> snapshots)
    {
        if (state->deviceError.code != GpuErrorCode::NONE)
            return FrameError{FrameErrorCode::DEVICE_LOST, {}, state->deviceError};
        if (state->stopping || lease.context != state->context.GetIdentity() || !lease.generation || snapshots.size() > 64)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        const auto ready = state->context.AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING);
        if (const auto *error = std::get_if<FrameError>(&ready))
            return *error;
        OutputAcquireBatch batch;
        for (const auto &snapshot : snapshots) {
            auto *output = state->Find(snapshot.viewport);
            if (!output || output->closing || output->failed || output->resizePending || output->snapshot.revision != snapshot.revision ||
                output->snapshot.extent.width != snapshot.extent.width || output->snapshot.extent.height != snapshot.extent.height ||
                output->snapshot.transform != snapshot.transform || output->snapshot.surfaceRevision != snapshot.surfaceRevision ||
                output->surfaceStatus != PresentationStatus::READY || !snapshot.extent.width || !snapshot.extent.height ||
                std::any_of(output->frames.begin(), output->frames.end(), [&](const auto &frame) { return frame.lease == lease; })) {
                batch.skipped.push_back({snapshot.viewport});
                continue;
            }
            const auto actual = output->swapchain->GetExtent();
            if (actual.width != snapshot.extent.width || actual.height != snapshot.extent.height) {
                batch.skipped.push_back({snapshot.viewport});
                continue;
            }
            const auto acquired = output->swapchain->TryAcquireFrame(0);
            if (acquired.status != PresentationStatus::READY) {
                PresentationResult failure = acquired;
                if (acquired.status == PresentationStatus::OUT_OF_DATE) {
                    const auto queried    = state->Refresh(*output);
                    output->resizePending = true;
                    if (queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST)
                        failure = queried;
                }
                if (failure.error.code == GpuErrorCode::DEVICE_LOST) {
                    state->deviceError = failure.error;
                    Abandon(lease);
                    return FrameError{FrameErrorCode::DEVICE_LOST, {}, failure.error};
                }
                batch.skipped.push_back(
                    {snapshot.viewport,
                     failure.status == PresentationStatus::FAILED || failure.status == PresentationStatus::SURFACE_LOST ? OutputProgressKind::FAILED
                                                                                                                        : OutputProgressKind::SKIPPED,
                     failure.error,
                     failure.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE : OutputFailureDisposition::RETRY});
                if (failure.status == PresentationStatus::SURFACE_LOST)
                    output->failed = true;
                continue;
            }
            auto owner = std::make_shared<SwapChainPtr>(output->swapchain);
            output->frames.push_back({acquired.frame, lease, owner});
            const auto extent  = output->swapchain->GetExtent();
            const auto queried = state->Refresh(*output);
            if (queried.status != PresentationStatus::READY || output->snapshot.revision != snapshot.revision ||
                extent.width != snapshot.extent.width || extent.height != snapshot.extent.height) {
                output->frames.back().phase = State::Phase::ABANDONED;
                if (queried.error.code == GpuErrorCode::DEVICE_LOST) {
                    Abandon(lease);
                    return FrameError{FrameErrorCode::DEVICE_LOST, {}, queried.error};
                }
                batch.skipped.push_back(
                    {snapshot.viewport,
                     queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST ? OutputProgressKind::FAILED
                                                                                                                        : OutputProgressKind::SKIPPED,
                     queried.error,
                     queried.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE : OutputFailureDisposition::RETRY});
                continue;
            }
            FrameGraphImage image;
            image.image             = FrameImageReference::Borrow(output->swapchain->GetImage(acquired.frame.imageIndex), lease, std::move(owner));
            image.descriptor.format = output->swapchain->GetFormat();
            image.descriptor.extent = {extent.width, extent.height, 1};
            image.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_DST;
            image.initial           = ImageLayout::UNDEFINED;
            image.final             = ImageLayout::PRESENT;
            batch.outputs.push_back({snapshot, std::move(image)});
        }
        return batch;
    }
    FrameSubmitResult FrameScheduler::Submit(std::unique_ptr<RecordedFrame> &recorded, ResourceUploader *uploads, const Uuid &batch)
    {
        if (state->deviceError.code != GpuErrorCode::NONE)
            return SubmissionRejected{FrameError{FrameErrorCode::DEVICE_LOST, {}, state->deviceError}};
        if (!recorded)
            return SubmissionRejected{FrameError{FrameErrorCode::INVALID_ARGUMENT}};
        const auto lease = recorded->GetIdentity().lease;
        if (lease.context != state->context.GetIdentity())
            return SubmissionRejected{FrameError{FrameErrorCode::STALE_LEASE}};
        const auto                       *receipt = recorded->GetGraphReceipt();
        std::optional<FrameGraphAnalysis> abandonedAnalysis;
        if (receipt && receipt->graph && std::any_of(state->outputs.begin(), state->outputs.end(), [&](const auto &output) {
                return std::any_of(output.frames.begin(), output.frames.end(),
                                   [&](const auto &frame) { return frame.lease == lease && frame.phase == State::Phase::ABANDONED; });
            })) {
            auto analyzed = AnalyzeFrameGraph(*receipt->graph);
            if (const auto *error = std::get_if<FrameError>(&analyzed))
                return SubmissionRejected{*error};
            abandonedAnalysis = std::move(std::get<FrameGraphAnalysis>(analyzed));
        }
        if (receipt && receipt->graph)
            for (const auto &image : receipt->graph->GetData().images)
                if (image.image.IsBorrowed()) {
                    bool active    = false;
                    bool abandoned = false;
                    for (const auto &output : state->outputs)
                        for (const auto &frame : output.frames) {
                            const bool matches = frame.lease == lease && image.image.Get() == output.swapchain->GetImage(frame.native.imageIndex);
                            active |= matches && frame.phase == State::Phase::ACQUIRED;
                            abandoned |= matches && frame.phase == State::Phase::ABANDONED;
                        }
                    if (abandoned && abandonedAnalysis) {
                        const auto index = static_cast<uint32_t>(&image - receipt->graph->GetData().images.data());
                        active |= std::find(abandonedAnalysis->usedImages.begin(), abandonedAnalysis->usedImages.end(), index) ==
                                  abandonedAnalysis->usedImages.end();
                    }
                    if (!active)
                        return SubmissionRejected{FrameError{FrameErrorCode::STALE_LEASE}};
                }
        FrameSubmissionSync sync;
        auto                owners = std::make_shared<std::vector<SwapChainPtr>>();
        for (auto &output : state->outputs) {
            for (auto &frame : output.frames) {
                if (frame.lease != lease || frame.phase != State::Phase::ACQUIRED)
                    continue;
                if (!receipt || !receipt->graph)
                    return SubmissionRejected{FrameError{FrameErrorCode::INVALID_STATE}};
                const auto &data     = receipt->graph->GetData();
                const auto  exported = std::any_of(data.imageExports.begin(), data.imageExports.end(), [&](const auto handle) {
                    const auto &image = data.images[handle.index];
                    return image.image.IsBorrowed() && image.image.ValidFor(lease) &&
                           image.image.Get() == output.swapchain->GetImage(frame.native.imageIndex) && image.final == ImageLayout::PRESENT;
                });
                if (!exported)
                    return SubmissionRejected{FrameError{FrameErrorCode::INVALID_STATE, "Acquired output is missing from the recorded exports"}};
                if (frame.native.acquired)
                    sync.waits.push_back({frame.native.acquired, 0, PipelineStageBit::TOP});
                if (frame.native.rendered)
                    sync.signals.push_back({frame.native.rendered, 0, PipelineStageBit::BOTTOM});
                owners->push_back(output.swapchain);
            }
        }
        sync.owner            = owners;
        auto        result    = uploads ? uploads->Submit(batch, recorded, sync) : state->context.Submit(recorded, sync);
        const auto *accepted  = std::get_if<SubmissionAccepted>(&result);
        const auto *uncertain = std::get_if<SubmissionIndeterminate>(&result);
        if (accepted || uncertain)
            for (auto &output : state->outputs)
                for (auto &frame : output.frames)
                    if (frame.lease == lease && frame.phase == State::Phase::ACQUIRED) {
                        frame.token = accepted ? accepted->token : uncertain->token;
                        frame.phase = accepted ? State::Phase::SUBMITTED : State::Phase::QUARANTINED;
                    }
        return result;
    }
    void FrameScheduler::Retain(const DeviceFrameLease &lease, std::shared_ptr<const void> owner)
    {
        for (auto &output : state->outputs)
            for (auto &frame : output.frames)
                if (frame.lease == lease)
                    frame.retained = owner;
    }
    void FrameScheduler::Abandon(const DeviceFrameLease &lease)
    {
        for (auto &output : state->outputs)
            for (auto &frame : output.frames)
                if (frame.lease == lease && frame.phase == State::Phase::ACQUIRED)
                    frame.phase = State::Phase::ABANDONED;
    }
    void FrameScheduler::AbandonOutput(const DeviceFrameLease &lease, ViewportId id)
    {
        if (auto *output = state->Find(id))
            for (auto &frame : output->frames)
                if (frame.lease == lease && frame.phase == State::Phase::ACQUIRED)
                    frame.phase = State::Phase::ABANDONED;
    }
    void FrameScheduler::OnCompleted(std::span<const FrameCompletion> completions)
    {
        for (const auto &completion : completions)
            for (auto &output : state->outputs)
                for (auto &frame : output.frames)
                    if (frame.token.serial && frame.token == completion.token) {
                        frame.gpuComplete = completion.status == FrameCompletionStatus::COMPLETE;
                        if (!frame.gpuComplete)
                            frame.phase = State::Phase::QUARANTINED;
                    }
    }
    size_t FrameScheduler::Pump(std::span<OutputProgress> events)
    {
        size_t count = 0;
        for (auto output = state->outputs.begin(); output != state->outputs.end();) {
            auto report = [&](GpuError error, OutputFailureDisposition disposition = OutputFailureDisposition::REPLACE) {
                if (count == events.size())
                    return false;
                events[count++] = {output->snapshot.viewport, OutputProgressKind::FAILED, error, disposition};
                output->failed  = true;
                if (error.code == GpuErrorCode::DEVICE_LOST)
                    state->deviceError = error;
                return true;
            };
            for (auto frame = output->frames.begin(); frame != output->frames.end();) {
                if (output->closing && frame->phase == State::Phase::ACQUIRED && frame->borrower.expired())
                    frame->phase = State::Phase::ABANDONED;
                if (frame->phase == State::Phase::SUBMITTED && count < events.size()) {
                    const auto presented = output->swapchain->TryPresentFrame(frame->native.serial, frame->gpuComplete);
                    if (presented.status == PresentationStatus::FAILED) {
                        frame->phase = State::Phase::QUARANTINED;
                        report(presented.error, OutputFailureDisposition::SHUTDOWN);
                    } else if (presented.status != PresentationStatus::BUSY) {
                        frame->phase = State::Phase::PRESENTED;
                        if (presented.status == PresentationStatus::SURFACE_LOST)
                            report(presented.error);
                        if (presented.status == PresentationStatus::OUT_OF_DATE) {
                            const auto queried = state->Refresh(*output);
                            if (queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST)
                                events[count++] = {output->snapshot.viewport, OutputProgressKind::FAILED, queried.error,
                                                   queried.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE
                                                                                                      : OutputFailureDisposition::RETRY};
                            output->resizePending = true;
                        }
                    }
                }
                if (frame->borrower.expired() &&
                    (frame->phase == State::Phase::ABANDONED || (frame->phase == State::Phase::PRESENTED && frame->gpuComplete)) &&
                    count < events.size()) {
                    const auto retired = output->swapchain->RetireFrame(frame->native.serial, frame->phase == State::Phase::ABANDONED);
                    if (retired.status == FenceStatus::SIGNALED) {
                        frame = output->frames.erase(frame);
                        continue;
                    }
                    if (retired.status == FenceStatus::FAILED) {
                        frame->phase = State::Phase::QUARANTINED;
                        report(retired.error, OutputFailureDisposition::SHUTDOWN);
                    }
                }
                ++frame;
            }
            if (output->frames.empty() && count < events.size()) {
                if (output->closing) {
                    const auto closed = output->swapchain->TryClose();
                    if (closed.status == PresentationStatus::READY) {
                        events[count++] = {output->snapshot.viewport, OutputProgressKind::CLOSED};
                        output          = state->outputs.erase(output);
                        continue;
                    }
                    if (closed.status == PresentationStatus::FAILED && !output->failed)
                        report(closed.error, OutputFailureDisposition::SHUTDOWN);
                } else if (!output->failed && state->deviceError.code == GpuErrorCode::NONE) {
                    const auto extent = output->snapshot.extent;
                    const auto actual = output->swapchain->GetExtent();
                    if (extent.width && extent.height && (output->resizePending || extent.width != actual.width || extent.height != actual.height)) {
                        const auto queried = state->Refresh(*output);
                        if (queried.status != PresentationStatus::READY && queried.status != PresentationStatus::OUT_OF_DATE) {
                            if (queried.status == PresentationStatus::FAILED || queried.status == PresentationStatus::SURFACE_LOST)
                                events[count++] = {output->snapshot.viewport, OutputProgressKind::FAILED, queried.error,
                                                   queried.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE
                                                                                                      : OutputFailureDisposition::RETRY};
                            ++output;
                            continue;
                        }
                        const auto resized = output->swapchain->TryResize(queried.configuration.extent.width, queried.configuration.extent.height);
                        if (resized.status == PresentationStatus::READY) {
                            output->resizePending = false;
                            const auto updated    = state->Refresh(*output);
                            if (updated.status == PresentationStatus::FAILED || updated.status == PresentationStatus::SURFACE_LOST)
                                events[count++] = {output->snapshot.viewport, OutputProgressKind::FAILED, updated.error,
                                                   updated.status == PresentationStatus::SURFACE_LOST ? OutputFailureDisposition::REPLACE
                                                                                                      : OutputFailureDisposition::RETRY};
                        }
                        if (resized.status == PresentationStatus::FAILED || resized.status == PresentationStatus::SURFACE_LOST)
                            report(resized.error);
                    }
                }
            }
            ++output;
        }
        return count;
    }
    bool FrameScheduler::Empty() const noexcept
    {
        return state->outputs.empty();
    }
} // namespace sky::aurora
