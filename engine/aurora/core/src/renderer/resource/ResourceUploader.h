//
// Created on 2026/10/16.
//

#pragma once

#include "resource/ResourceCatalog.h"
#include <aurora/rdg/DeviceFrameContext.h>
#include <span>

namespace sky::aurora {
    class Device;
    class FrameGraphBuilder;

    struct ResourceUploadLimits {
        uint64_t bytesPerFrame        = 4 * 1024 * 1024;
        uint64_t inflightBytes        = 16 * 1024 * 1024;
        uint32_t createsPerFrame      = 64;
        uint32_t pendingBatches       = 4;
        uint32_t pendingResources     = 64;
        uint64_t pendingResourceBytes = 256ull * 1024 * 1024;
    };
    enum class UploadDeferReason : uint8_t { NOT_READY, FRAME_BYTES, CREATES, INFLIGHT, STAGING, RESOURCE_COUNT, RESOURCE_BYTES, STATE_TRACKING };
    struct DeferredResourceUpload {
        ResourceTicket    ticket;
        UploadDeferReason reason = UploadDeferReason::NOT_READY;
    };
    struct ResourceUploadBatch {
        Uuid                                id;
        std::vector<ResourceTicket>         admitted;
        std::vector<DeferredResourceUpload> deferred;
    };
    using ResourceUploadResult = std::variant<FrameError, ResourceUploadBatch>;

    // Coordinator-only. Device, context and catalog outlive this owner; its PollCompleted is the sole context poller.
    class ResourceUploader {
    public:
        static std::variant<FrameError, std::unique_ptr<ResourceUploader>>
        Create(Device &, DeviceFrameContext &, ResourceCatalog &, const ResourceUploadLimits & = {});
        ~ResourceUploader();
        // On error discard this graph/lease after encoding borrowers drain; catalog CPU data remains retryable.
        ResourceUploadResult Prepare(const DeviceFrameLease &, FrameGraphBuilder &, std::span<const ResourceTicket>);
        std::variant<FrameError, GraphBufferHandle, GraphImageHandle>
                                               ResolveInGraph(const Uuid &, const ResourceTicket &, const FrameGraphBuilder &) const;
        std::shared_ptr<const ResourceBacking> ResolveBackingInGraph(const Uuid &, const ResourceTicket &, const FrameGraphBuilder &) const;
        FrameSubmitResult                      Submit(const Uuid &, std::unique_ptr<RecordedFrame> &, const FrameSubmissionSync & = {});
        FrameOperationResult                   Discard(const Uuid &);
        size_t                                 PollCompleted(std::span<FrameCompletion>);
        uint64_t                               InflightBytes() const noexcept;
        uint64_t                               PendingResourceBytes() const noexcept;

    private:
        ResourceUploader();
        struct State;
        std::unique_ptr<State> state;
    };
} // namespace sky::aurora
