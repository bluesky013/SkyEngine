//
// Created on 2026/10/16.
//

#include "resource/ResourceCatalog.h"

#include "scene/CpuSceneStore.h"
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace sky::aurora {
    namespace {
        struct KeyLess {
            bool operator()(const RenderResourceKey &a, const RenderResourceKey &b) const noexcept
            {
                return std::tie(a.product, a.usageVariant, a.deviceEpoch, a.revision) <
                       std::tie(b.product, b.usageVariant, b.deviceEpoch, b.revision);
            }
        };
        using SubscriberSlot = std::tuple<uint32_t, uint32_t, ResourceBindingSlot>;
        SubscriberSlot Slot(const ResourceSubscriber &subscriber)
        {
            return {subscriber.entity.scene.index, subscriber.entity.entity.index, subscriber.slot};
        }
        const RenderResourceBinding *Current(const CpuSceneStore &store, const ResourceSubscriber &subscriber)
        {
            const auto *scene  = store.FindScene(subscriber.entity.scene);
            const auto *entity = scene ? scene->FindEntity(subscriber.entity.entity) : nullptr;
            if (!entity || subscriber.slot > ResourceBindingSlot::MATERIAL)
                return nullptr;
            const auto *mesh     = scene->registry.Get<RenderMeshBinding>(entity->entity);
            const auto *material = scene->registry.Get<RenderMaterialBinding>(entity->entity);
            const auto *binding =
                subscriber.slot == ResourceBindingSlot::MESH ? (mesh ? &mesh->binding : nullptr) : (material ? &material->binding : nullptr);
            return binding && binding->bindingRevision == subscriber.bindingRevision ? binding : nullptr;
        }
        bool Compatible(const RenderResourceKey &candidate, const RenderResourceKey &desired)
        {
            return candidate.product == desired.product && candidate.usageVariant == desired.usageVariant &&
                   candidate.deviceEpoch == desired.deviceEpoch && candidate.revision <= desired.revision;
        }
        RendererError Stale()
        {
            return {RendererErrorCode::STALE_HANDLE, "Resource ticket or subscription is stale"};
        }
        RendererError Invalid()
        {
            return {RendererErrorCode::INVALID_ARGUMENT, "Invalid resource request or state"};
        }
        RendererError Full()
        {
            return {RendererErrorCode::OUT_OF_MEMORY, "Resource catalog budget exceeded"};
        }

    } // namespace

    struct ResourceCatalog::State {
        struct Entry {
            ResourceTicket                               ticket;
            ResourceCatalogStatus                        status;
            Name                                         format;
            uint32_t                                     subscribers = 0;
            bool                                         standalone  = false;
            bool                                         released    = false;
            std::shared_ptr<const RenderResourcePayload> payload;
            std::shared_ptr<const ResourceVersionData>   candidate;
            std::shared_ptr<const ResourceVersionLease>  resident;
        };
        struct Subscription {
            ResourceSubscriber subscriber;
            RenderResourceKey  desired;
            bool               active = false;
        };
        ResourceCatalogInit                         limits;
        Uuid                                        identity;
        uint64_t                                    sequence = 0;
        uint64_t                                    cpuBytes = 0;
        std::map<RenderResourceKey, Entry, KeyLess> entries;
        std::map<SubscriberSlot, Subscription>      subscriptions;

        Entry *Find(const ResourceTicket &ticket)
        {
            const auto it = entries.find(ticket.resource);
            return it != entries.end() && it->second.ticket == ticket ? &it->second : nullptr;
        }
        const Entry *Find(const ResourceTicket &ticket) const
        {
            const auto it = entries.find(ticket.resource);
            return it != entries.end() && it->second.ticket == ticket ? &it->second : nullptr;
        }
        void DropPayload(Entry &entry)
        {
            if (entry.payload) {
                cpuBytes -= RenderResourceCpuBytes(*entry.payload);
                entry.payload.reset();
            }
        }
        void Deactivate(Subscription &subscription)
        {
            if (!subscription.active)
                return;
            subscription.active = false;
            auto &entry         = entries.at(subscription.desired);
            --entry.subscribers;
            if (!entry.subscribers && !entry.standalone &&
                (entry.status.state == RenderResourceState::REQUESTED || entry.status.state == RenderResourceState::CPU_READY ||
                 entry.status.state == RenderResourceState::UPLOAD_QUEUED || entry.status.state == RenderResourceState::UPLOAD_SUBMITTED)) {
                DropPayload(entry);
                entry.status.state = RenderResourceState::CANCELLED;
            }
        }
        void Refresh(const CpuSceneStore &store)
        {
            for (auto &[slot, subscription] : subscriptions) {
                if (!subscription.active)
                    continue;
                const auto *current = Current(store, subscription.subscriber);
                if (!current || current->resource != subscription.desired)
                    Deactivate(subscription);
            }
        }
        std::shared_ptr<const ResourceVersionLease> Select(const RenderResourceKey &desired) const
        {
            const auto exact = entries.find(desired);
            auto       it    = entries.upper_bound(desired);
            while (it != entries.begin()) {
                const auto &[key, entry] = *--it;
                if (!Compatible(key, desired))
                    break;
                if (!entry.resident)
                    continue;
                if (exact != entries.end() && !exact->second.format.Empty() && entry.resident->cpu->format != exact->second.format)
                    continue;
                if (exact != entries.end() && exact->second.candidate &&
                    bool(entry.resident->cpu->metadata.image) != bool(exact->second.candidate->metadata.image))
                    continue;
                return entry.resident;
            }
            return {};
        }
    };

    ResourceCatalog::ResourceCatalog() : state(std::make_unique<State>())
    {
    }
    ResourceCatalog::~ResourceCatalog() = default;
    std::variant<RendererError, std::unique_ptr<ResourceCatalog>> ResourceCatalog::Create(const ResourceCatalogInit &info)
    {
        if (!info.deviceEpoch || !info.maxCpuBytes || !info.maxTickets || !info.maxSubscribers)
            return Invalid();
        auto catalog             = std::unique_ptr<ResourceCatalog>(new ResourceCatalog());
        catalog->state->limits   = info;
        catalog->state->identity = Uuid::Create();
        if (!catalog->state->identity)
            return RendererError{RendererErrorCode::INTERNAL_ERROR, "Could not allocate catalog identity"};
        return catalog;
    }
    std::variant<RendererError, ResourceRequest>
    ResourceCatalog::Request(const CpuSceneStore &store, const ResourceSubscriber &subscriber, bool retry)
    {
        const auto *current = Current(store, subscriber);
        if (!current)
            return Stale();
        const auto &key = current->resource;
        if (!subscriber.requestSequence || !key.product || !key.revision || key.deviceEpoch != state->limits.deviceEpoch)
            return Invalid();
        const auto found = state->subscriptions.find(Slot(subscriber));
        if (found != state->subscriptions.end() && found->second.subscriber.entity == subscriber.entity &&
            subscriber.requestSequence <= found->second.subscriber.requestSequence) {
            if (!retry && found->second.active && found->second.subscriber == subscriber && found->second.desired == key)
                return ResourceRequest{state->entries.at(key).ticket, false};
            return Stale();
        }
        auto       entry   = state->entries.find(key);
        const bool restart = entry != state->entries.end() && (entry->second.status.state == RenderResourceState::FAILED ||
                                                               entry->second.status.state == RenderResourceState::CANCELLED);
        if (restart && !retry)
            return RendererError{RendererErrorCode::NOT_READY, "Failed or cancelled resources require an explicit retry"};
        const bool create = entry == state->entries.end() || restart;
        if ((entry == state->entries.end() && state->entries.size() >= state->limits.maxTickets) ||
            (found == state->subscriptions.end() && state->subscriptions.size() >= state->limits.maxSubscribers) ||
            (create && state->sequence == UINT64_MAX))
            return Full();
        if (create) {
            auto      &target      = state->entries[key];
            const auto format      = target.format;
            const auto subscribers = target.subscribers;
            state->DropPayload(target);
            target             = {};
            target.format      = format;
            target.subscribers = subscribers;
            target.ticket      = {state->identity, ++state->sequence, key};
            entry              = state->entries.find(key);
        }
        ++entry->second.subscribers;
        entry->second.standalone = false;
        if (found != state->subscriptions.end())
            state->Deactivate(found->second);
        state->subscriptions[Slot(subscriber)] = {subscriber, key, true};
        return ResourceRequest{entry->second.ticket, create};
    }
    std::variant<RendererError, ResourceRequest> ResourceCatalog::Request(const RenderResourceKey &key, bool retry)
    {
        if (!key.product || !key.revision || key.deviceEpoch != state->limits.deviceEpoch)
            return Invalid();
        const auto found = state->entries.find(key);
        if (found != state->entries.end()) {
            auto &entry = found->second;
            if (entry.status.state != RenderResourceState::FAILED && entry.status.state != RenderResourceState::CANCELLED)
                return ResourceRequest{entry.ticket, entry.status.state == RenderResourceState::REQUESTED};
            if (!retry)
                return RendererError{RendererErrorCode::NOT_READY, "Failed or cancelled resources require an explicit retry"};
            if (state->sequence == UINT64_MAX)
                return Full();
            const auto format      = entry.format;
            const auto subscribers = entry.subscribers;
            const auto standalone  = entry.standalone;
            state->DropPayload(entry);
            entry             = {};
            entry.format      = format;
            entry.subscribers = subscribers;
            entry.standalone  = standalone;
            entry.ticket      = {state->identity, ++state->sequence, key};
            return ResourceRequest{entry.ticket, true};
        }
        if (state->entries.size() >= state->limits.maxTickets || state->sequence == UINT64_MAX)
            return Full();
        auto &entry      = state->entries[key];
        entry.ticket     = {state->identity, ++state->sequence, key};
        entry.standalone = true;
        return ResourceRequest{entry.ticket, true};
    }
    ResourceCatalogResult ResourceCatalog::Cancel(const ResourceSubscriber &subscriber)
    {
        const auto found = state->subscriptions.find(Slot(subscriber));
        if (found == state->subscriptions.end() || found->second.subscriber != subscriber)
            return Stale();
        state->Deactivate(found->second);
        return {};
    }
    ResourceCatalogResult ResourceCatalog::Supply(const ResourceTicket &ticket, RenderResourcePayload &payload)
    {
        auto *entry = state->Find(ticket);
        if (!entry)
            return Stale();
        if (entry->status.state != RenderResourceState::REQUESTED || payload.resource != ticket.resource || payload.format.Empty() ||
            (!entry->format.Empty() && payload.format != entry->format) || !IsValidRenderResourceMetadata(payload.metadata))
            return Invalid();
        if (payload.metadata.image && RenderImageByteSize(*payload.metadata.image) != payload.bytes.size())
            return Invalid();
        if (RenderResourceCpuBytes(payload) > state->limits.maxCpuBytes - state->cpuBytes)
            return Full();
        auto owned = std::make_shared<const RenderResourcePayload>(std::move(payload));
        state->cpuBytes += RenderResourceCpuBytes(*owned);
        entry->format    = owned->format;
        entry->candidate = std::make_shared<const ResourceVersionData>(ResourceVersionData{ticket.resource, owned->format, owned->metadata});
        entry->status.upload.totalBytes = owned->bytes.size();
        entry->payload                  = std::move(owned);
        entry->status.state             = RenderResourceState::CPU_READY;
        return {};
    }
    ResourceCatalogResult ResourceCatalog::Fail(const ResourceTicket &ticket, RendererError error)
    {
        auto *entry = state->Find(ticket);
        if (!entry)
            return Stale();
        if (entry->status.state != RenderResourceState::REQUESTED && entry->status.state != RenderResourceState::CPU_READY &&
            entry->status.state != RenderResourceState::UPLOAD_QUEUED && entry->status.state != RenderResourceState::UPLOAD_SUBMITTED)
            return Invalid();
        state->DropPayload(*entry);
        entry->status.state = RenderResourceState::FAILED;
        entry->status.error = std::move(error);
        return {};
    }
    std::variant<RendererError, ResourceCatalogStatus> ResourceCatalog::GetStatus(const ResourceTicket &ticket) const
    {
        const auto *entry = state->Find(ticket);
        return entry ? std::variant<RendererError, ResourceCatalogStatus>{entry->status} : Stale();
    }
    std::shared_ptr<const RenderResourcePayload> ResourceCatalog::BorrowPayload(const ResourceTicket &ticket) const
    {
        const auto *entry = state->Find(ticket);
        return entry ? entry->payload : nullptr;
    }
    ResourceCatalogResult ResourceCatalog::AdoptResident(const ResourceTicket                  &ticket,
                                                         const RenderResourceKey               &backingVersion,
                                                         std::shared_ptr<const ResourceBacking> backing)
    {
        auto *entry = state->Find(ticket);
        if (!entry)
            return Stale();
        if (!backing || backingVersion != ticket.resource ||
            (entry->status.state != RenderResourceState::CPU_READY && entry->status.state != RenderResourceState::UPLOAD_SUBMITTED))
            return Invalid();
        if (entry->status.state == RenderResourceState::UPLOAD_SUBMITTED && entry->status.upload.completedBytes != entry->status.upload.totalBytes)
            return Invalid();
        const auto *buffer = std::get_if<ResourceBufferBacking>(backing.get());
        const auto *image  = std::get_if<ResourceImageBacking>(backing.get());
        if (bool(image) != bool(entry->candidate->metadata.image))
            return Invalid();
        // CPU-only probes may publish an empty backing before any upload.
        if (entry->status.state == RenderResourceState::CPU_READY &&
            (entry->status.upload.submittedBytes || entry->status.upload.completedBytes || (buffer && buffer->buffer)))
            return Invalid();
        if (image) {
            const auto &layout = *entry->candidate->metadata.image;
            const auto &desc   = image->descriptor;
            const auto  format = layout.format == RenderImageFormat::RGBA16_FLOAT ? PixelFormat::RGBA16_SFLOAT : PixelFormat::RGBA8_UNORM;
            if (!image->image || !image->tracking || image->state != ImageLayout::GENERAL || desc.imageType != ImageType::IMAGE_2D ||
                desc.extent.width != layout.width || desc.extent.height != layout.height || desc.extent.depth != 1 ||
                desc.mipLevels != layout.mipLevels || desc.arrayLayers != layout.arrayLayers || desc.format != format ||
                desc.samples != SampleCount::X1 || entry->status.state != RenderResourceState::UPLOAD_SUBMITTED ||
                image->initializedBy != entry->status.submission)
                return Invalid();
        } else if (entry->status.state == RenderResourceState::UPLOAD_SUBMITTED &&
                   (!buffer->buffer || !buffer->tracking || buffer->state != CopyBufferState::COMMON ||
                    buffer->initializedBy != entry->status.submission)) {
            return Invalid();
        }
        entry->resident = std::make_shared<const ResourceVersionLease>(ResourceVersionLease{entry->candidate, std::move(backing)});
        state->DropPayload(*entry);
        entry->status.state                 = RenderResourceState::RESIDENT;
        entry->status.upload.submittedBytes = entry->status.upload.totalBytes;
        entry->status.upload.completedBytes = entry->status.upload.totalBytes;
        return {};
    }
    FrozenResourceCatalog ResourceCatalog::Freeze(const CpuSceneStore &store, bool includeCandidates, std::span<const RenderResourceKey> external)
    {
        state->Refresh(store);
        FrozenResourceCatalog                result;
        auto                                 cpu = std::make_shared<std::vector<ResolvedResourceBinding>>();
        std::set<RenderResourceKey, KeyLess> pinned;
        for (const auto &[slot, subscription] : state->subscriptions) {
            if (!subscription.active)
                continue;
            auto version = state->Select(subscription.desired);
            if (!version && includeCandidates) {
                const auto found = state->entries.find(subscription.desired);
                if (found != state->entries.end() && found->second.candidate && found->second.payload &&
                    found->second.status.state == RenderResourceState::CPU_READY)
                    version = std::make_shared<const ResourceVersionLease>(ResourceVersionLease{found->second.candidate, {}});
            }
            cpu->push_back({subscription.subscriber, subscription.desired, version ? version->cpu : nullptr});
            if (version && pinned.insert(version->cpu->resource).second)
                result.pins.push_back(std::move(version));
        }
        for (const auto &key : external) {
            const auto found = state->entries.find(key);
            if (found == state->entries.end())
                continue;
            auto version = found->second.resident;
            if (!version && includeCandidates && found->second.candidate && found->second.payload)
                version = std::make_shared<const ResourceVersionLease>(ResourceVersionLease{found->second.candidate, {}});
            if (version && pinned.insert(key).second)
                result.pins.push_back(std::move(version));
        }
        result.cpu = std::move(cpu);
        return result;
    }
    std::shared_ptr<const ResourceVersionLease> ResourceCatalog::ResolveResident(const RenderResourceKey &key) const
    {
        const auto found = state->entries.find(key);
        return found != state->entries.end() ? found->second.resident : nullptr;
    }
    void ResourceCatalog::Release(const RenderResourceKey &key)
    {
        const auto found = state->entries.find(key);
        if (found != state->entries.end()) {
            found->second.standalone = false;
            found->second.released   = true;
        }
    }
    size_t ResourceCatalog::Prune(const CpuSceneStore &store)
    {
        state->Refresh(store);
        std::set<RenderResourceKey, KeyLess> keep;
        for (const auto &[slot, subscription] : state->subscriptions) {
            if (!subscription.active)
                continue;
            keep.insert(subscription.desired);
            if (auto version = state->Select(subscription.desired))
                keep.insert(version->cpu->resource);
        }
        size_t removed = 0;
        for (auto it = state->entries.begin(); it != state->entries.end();) {
            // Candidate has one catalog owner, plus one resident owner; extra owners belong to queued frames.
            if (keep.contains(it->first) || it->second.standalone ||
                (it->second.released &&
                 (it->second.candidate.use_count() > (it->second.resident ? 2 : 1) || it->second.status.state == RenderResourceState::UPLOAD_QUEUED ||
                  it->second.status.state == RenderResourceState::UPLOAD_SUBMITTED))) {
                ++it;
            } else {
                state->DropPayload(it->second);
                it = state->entries.erase(it);
                ++removed;
            }
        }
        return removed;
    }
    uint64_t ResourceCatalog::CpuBytes() const noexcept
    {
        return state->cpuBytes;
    }
    uint64_t ResourceCatalog::DeviceEpoch() const noexcept
    {
        return state->limits.deviceEpoch;
    }
    bool ResourceCatalog::QueueUpload(const ResourceTicket &ticket)
    {
        auto *entry = state->Find(ticket);
        if (!entry || entry->status.state != RenderResourceState::CPU_READY ||
            entry->status.upload.submittedBytes != entry->status.upload.completedBytes ||
            entry->status.upload.completedBytes >= entry->status.upload.totalBytes)
            return false;
        entry->status.state = RenderResourceState::UPLOAD_QUEUED;
        return true;
    }
    void ResourceCatalog::RollbackUpload(const ResourceTicket &ticket)
    {
        auto *entry = state->Find(ticket);
        if (entry && entry->status.state == RenderResourceState::UPLOAD_QUEUED)
            entry->status.state = RenderResourceState::CPU_READY;
    }
    void ResourceCatalog::AcceptUpload(const ResourceTicket &ticket, const SubmissionToken &submission, uint64_t bytes)
    {
        auto *entry = state->Find(ticket);
        if (entry && entry->status.state == RenderResourceState::UPLOAD_QUEUED) {
            entry->status.state                 = RenderResourceState::UPLOAD_SUBMITTED;
            entry->status.submission            = submission;
            entry->status.upload.submittedBytes = entry->status.upload.completedBytes + bytes;
            if (entry->status.upload.submittedBytes == entry->status.upload.totalBytes)
                state->DropPayload(*entry);
        }
    }
    bool ResourceCatalog::CompleteUpload(const ResourceTicket &ticket, const SubmissionToken &submission)
    {
        auto *entry = state->Find(ticket);
        if (!entry || entry->status.state != RenderResourceState::UPLOAD_SUBMITTED || entry->status.submission != submission)
            return false;
        entry->status.upload.completedBytes = entry->status.upload.submittedBytes;
        if (entry->status.upload.completedBytes < entry->status.upload.totalBytes) {
            entry->status.state      = RenderResourceState::CPU_READY;
            entry->status.submission = {};
        }
        return true;
    }
    void ResourceCatalog::RestartUpload(const ResourceTicket &ticket)
    {
        auto *entry = state->Find(ticket);
        if (entry && entry->status.state == RenderResourceState::CPU_READY) {
            entry->status.upload.submittedBytes = 0;
            entry->status.upload.completedBytes = 0;
            entry->status.submission            = {};
        }
    }
} // namespace sky::aurora
