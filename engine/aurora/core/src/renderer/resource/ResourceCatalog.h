//
// Created on 2026/10/16.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/renderer/RenderResourceTypes.h>
#include <memory>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace sky::aurora {
    class CpuSceneStore;
    class FrameResourceTracking;

    struct ResourceTicket {
        Uuid              catalog;
        uint64_t          sequence = 0;
        RenderResourceKey resource;
        bool              operator==(const ResourceTicket &) const = default;
    };
    struct ResourceRequest {
        ResourceTicket ticket;
        bool           needsCpuData = false;
    };
    struct ResourceBufferBacking {
        BufferPtr                                    buffer;
        CopyBufferState                              state = CopyBufferState::COMMON;
        SubmissionToken                              initializedBy;
        std::shared_ptr<const FrameResourceTracking> tracking;
    };
    struct ResourceImageBacking {
        ImagePtr                                     image;
        Image::Descriptor                            descriptor;
        ImageLayout                                  state = ImageLayout::GENERAL;
        SubmissionToken                              initializedBy;
        std::shared_ptr<const FrameResourceTracking> tracking;
    };
    using ResourceBacking = std::variant<ResourceBufferBacking, ResourceImageBacking>;
    struct ResourceVersionLease {
        std::shared_ptr<const ResourceVersionData> cpu;
        std::shared_ptr<const ResourceBacking>     backing;
    };
    struct FrozenResourceCatalog {
        // Prepare receives only cpu; native pins stay with the coordinator/submission.
        std::shared_ptr<const std::vector<ResolvedResourceBinding>> cpu;
        std::vector<std::shared_ptr<const ResourceVersionLease>>    pins;
    };
    struct ResourceCatalogInit {
        uint64_t deviceEpoch    = 0;
        uint64_t maxCpuBytes    = 256ull * 1024 * 1024; // Catalog ownership only; uploader budgets retained source bytes separately.
        uint32_t maxTickets     = 4096;
        uint32_t maxSubscribers = 65536;
    };
    struct ResourceUploadProgress {
        uint64_t totalBytes     = 0;
        uint64_t submittedBytes = 0;
        uint64_t completedBytes = 0;
    };
    struct ResourceCatalogStatus {
        RenderResourceState    state = RenderResourceState::REQUESTED;
        RendererError          error;
        SubmissionToken        submission;
        ResourceUploadProgress upload;
    };
    using ResourceCatalogResult = std::optional<RendererError>;

    // All mutations and ECS queries run on the coordinator; returned CPU data and version leases are immutable.
    class ResourceCatalog {
    public:
        static std::variant<RendererError, std::unique_ptr<ResourceCatalog>> Create(const ResourceCatalogInit &);
        ~ResourceCatalog();
        std::variant<RendererError, ResourceRequest>       Request(const CpuSceneStore &, const ResourceSubscriber &, bool retry = false);
        std::variant<RendererError, ResourceRequest>       Request(const RenderResourceKey &, bool retry = false);
        ResourceCatalogResult                              Cancel(const ResourceSubscriber &);
        ResourceCatalogResult                              Supply(const ResourceTicket &, RenderResourcePayload &);
        ResourceCatalogResult                              Fail(const ResourceTicket &, RendererError);
        std::variant<RendererError, ResourceCatalogStatus> GetStatus(const ResourceTicket &) const;
        std::shared_ptr<const RenderResourcePayload>       BorrowPayload(const ResourceTicket &) const;
        // Internal upload-owner seam: completion and final state must be proven before adoption.
        ResourceCatalogResult AdoptResident(const ResourceTicket &, const RenderResourceKey &backingVersion, std::shared_ptr<const ResourceBacking>);
        FrozenResourceCatalog Freeze(const CpuSceneStore &, bool includeCandidates = false, std::span<const RenderResourceKey> external = {});
        void                  Release(const RenderResourceKey &);
        std::shared_ptr<const ResourceVersionLease> ResolveResident(const RenderResourceKey &) const;
        size_t                                      Prune(const CpuSceneStore &);
        uint64_t                                    CpuBytes() const noexcept;
        uint64_t                                    DeviceEpoch() const noexcept;

    private:
        friend class ResourceUploader;
        bool QueueUpload(const ResourceTicket &);
        void RollbackUpload(const ResourceTicket &);
        void AcceptUpload(const ResourceTicket &, const SubmissionToken &, uint64_t bytes);
        bool CompleteUpload(const ResourceTicket &, const SubmissionToken &);
        void RestartUpload(const ResourceTicket &);
        ResourceCatalog();
        struct State;
        std::unique_ptr<State> state;
    };
} // namespace sky::aurora
