//
// Created on 2026/10/16.
//

#include "resource/ResourceUploader.h"

#include "graph/FrameGraphBuilder.h"
#include <algorithm>
#include <aurora/rhi/Device.h>
#include <map>

namespace sky::aurora {
    namespace {
        FrameError Invalid(const char *message)
        {
            return {FrameErrorCode::INVALID_STATE, message};
        }
        RendererError ResourceFailure(const FrameError &error)
        {
            return {error.code == FrameErrorCode::DEVICE_LOST || error.cause.code == GpuErrorCode::DEVICE_LOST ? RendererErrorCode::DEVICE_LOST
                                                                                                               : RendererErrorCode::INTERNAL_ERROR,
                    error.message.empty() ? "GPU upload failed or its completion is uncertain" : error.message};
        }
        bool Matches(const BufferImageCopyPass &a, const BufferImageCopyPass &b)
        {
            const auto &x = a.region;
            const auto &y = b.region;
            return a.buffer == b.buffer && a.image == b.image && a.direction == b.direction && a.sideEffect == b.sideEffect &&
                   x.bufferOffset == y.bufferOffset && x.bufferRowLength == y.bufferRowLength && x.bufferImageHeight == y.bufferImageHeight &&
                   x.subRange.level == y.subRange.level && x.subRange.baseLayer == y.subRange.baseLayer && x.subRange.layers == y.subRange.layers &&
                   x.subRange.aspectMask == y.subRange.aspectMask && x.imageOffset.x == y.imageOffset.x && x.imageOffset.y == y.imageOffset.y &&
                   x.imageOffset.z == y.imageOffset.z && x.imageExtent.width == y.imageExtent.width && x.imageExtent.height == y.imageExtent.height &&
                   x.imageExtent.depth == y.imageExtent.depth;
        }
        FrameResource TrackingResource(const ResourceBacking &backing)
        {
            if (const auto *buffer = std::get_if<ResourceBufferBacking>(&backing))
                return FrameResource{buffer->buffer};
            return FrameResource{std::get<ResourceImageBacking>(backing).image};
        }
        FrameGraphImage ImportImagePrefix(const ResourceImageBacking &backing, uint64_t bytes, const ImageBoundaryState &initial)
        {
            FrameGraphImage image{backing.image, backing.descriptor, initial, ImageLayout::GENERAL, false};
            const auto     &desc       = backing.descriptor;
            const uint32_t  texelBytes = desc.format == PixelFormat::RGBA16_SFLOAT ? 8 : 4;
            uint64_t        layerBytes = 0;
            for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
                layerBytes += uint64_t(std::max(1u, desc.extent.width >> mip)) * std::max(1u, desc.extent.height >> mip) * texelBytes;
            const auto layers = static_cast<uint32_t>(bytes / layerBytes);
            if (layers)
                image.initializedRanges.push_back({0, desc.mipLevels, 0, layers, AspectFlagBit::COLOR_BIT});
            bytes %= layerBytes;
            uint32_t mip = 0;
            for (; mip < desc.mipLevels && bytes; ++mip) {
                const auto width  = std::max(1u, desc.extent.width >> mip);
                const auto height = std::max(1u, desc.extent.height >> mip);
                const auto size   = uint64_t(width) * height * texelBytes;
                if (bytes < size) {
                    const auto rows = static_cast<uint32_t>(bytes / texelBytes / width);
                    const auto tail = static_cast<uint32_t>(bytes / texelBytes % width);
                    if (rows)
                        image.initializedRegions.push_back({mip, layers, {{0, 0}, {width, rows}}});
                    if (tail)
                        image.initializedRegions.push_back({mip, layers, {{0, static_cast<int32_t>(rows)}, {tail, 1}}});
                    break;
                }
                bytes -= size;
            }
            if (mip)
                image.initializedRanges.push_back({0, mip, layers, 1, AspectFlagBit::COLOR_BIT});
            return image;
        }
        std::vector<BufferImageCopyPass> PlanImageChunk(const RenderImageLayout &layout, uint64_t first, uint64_t &bytes, uint32_t passBudget)
        {
            std::vector<BufferImageCopyPass> copies;
            uint64_t                         offset     = 0;
            const auto                       texelBytes = RenderImageTexelBytes(layout.format);
            for (uint32_t layer = 0; layer < layout.arrayLayers && offset < bytes && copies.size() < passBudget; ++layer) {
                for (uint32_t mip = 0; mip < layout.mipLevels && offset < bytes && copies.size() < passBudget; ++mip) {
                    const auto width  = std::max(1u, layout.width >> mip);
                    const auto height = std::max(1u, layout.height >> mip);
                    const auto size   = uint64_t(width) * height * texelBytes;
                    if (first >= size) {
                        first -= size;
                        continue;
                    }
                    auto pixel = first / texelBytes;
                    first      = 0;
                    while (pixel < size / texelBytes && offset < bytes && copies.size() < passBudget) {
                        const auto          x         = static_cast<uint32_t>(pixel % width);
                        const auto          y         = static_cast<uint32_t>(pixel / width);
                        const auto          available = (bytes - offset) / texelBytes;
                        const auto          rows      = !x ? static_cast<uint32_t>(std::min<uint64_t>(height - y, available / width)) : 0;
                        const auto          columns   = rows ? width : static_cast<uint32_t>(std::min<uint64_t>(width - x, available));
                        BufferImageCopyPass copy;
                        copy.region.bufferOffset = offset;
                        copy.region.subRange     = {mip, layer, 1, AspectFlagBit::COLOR_BIT};
                        copy.region.imageOffset  = {static_cast<int32_t>(x), static_cast<int32_t>(y), 0};
                        copy.region.imageExtent  = {columns, rows ? rows : 1, 1};
                        copy.sideEffect          = true;
                        copies.push_back(copy);
                        const auto count = uint64_t(columns) * copy.region.imageExtent.height;
                        pixel += count;
                        offset += count * texelBytes;
                    }
                }
            }
            bytes = offset;
            return copies;
        }
    } // namespace

    struct ResourceUploader::State {
        struct Item {
            ResourceTicket                                                 ticket;
            std::vector<std::variant<BufferCopyPass, BufferImageCopyPass>> copies;
            ResourceBacking                                                backing;
            FrameResourceState                                             initial;
            BufferPtr                                                      staging;
            uint32_t                                                       pass  = 0;
            uint64_t                                                       first = 0;
            uint64_t                                                       bytes = 0;
            uint64_t                                                       total = 0;
        };
        struct Batch {
            DeviceFrameLease  lease;
            uint64_t          graph = 0;
            uint64_t          bytes = 0;
            std::vector<Item> items;
            SubmissionToken   submitted;
            bool              uncertain = false;
        };
        struct PendingResource {
            ResourceTicket  ticket;
            ResourceBacking backing;
            uint64_t        bytes   = 0;
            bool            pending = false;
        };
        std::map<uint64_t, PendingResource> resources;
        uint64_t                            resourceBytes = 0;
        Device                             *device        = nullptr;
        DeviceFrameContext                 *context       = nullptr;
        ResourceCatalog                    *catalog       = nullptr;
        ResourceUploadLimits                limits;
        std::map<Uuid, Batch>               batches;
        uint64_t                            reserved = 0;
        DeviceFrameLease                    budgetLease;
        uint64_t                            frameBytes   = 0;
        uint32_t                            frameCreates = 0;
        std::optional<FrameError>           terminal;

        void PruneResources()
        {
            for (auto it = resources.begin(); it != resources.end();) {
                const auto  status  = catalog->GetStatus(it->second.ticket);
                const auto *current = std::get_if<ResourceCatalogStatus>(&status);
                if (!it->second.pending && (!current || current->state != RenderResourceState::CPU_READY || !current->upload.completedBytes)) {
                    resourceBytes -= it->second.bytes;
                    it = resources.erase(it);
                } else {
                    ++it;
                }
            }
        }
        void Rollback(const Batch &batch)
        {
            for (const auto &item : batch.items) {
                catalog->RollbackUpload(item.ticket);
                if (auto resource = resources.find(item.ticket.sequence); resource != resources.end())
                    resource->second.pending = false;
            }
            PruneResources();
        }
        bool Verify(const Batch &batch, const RecordedFrame &recording) const
        {
            const auto *receipt = recording.GetGraphReceipt();
            if (recording.GetIdentity().lease != batch.lease || recording.GetIdentity().graphId != batch.graph || !receipt || !receipt->graph ||
                receipt->graph->GetId() != batch.graph)
                return false;
            const auto       &data = receipt->graph->GetData();
            std::vector<bool> executed(data.passes.size());
            for (const auto pass : receipt->passes) {
                if (pass >= executed.size() || executed[pass])
                    return false;
                executed[pass] = true;
            }
            for (const auto &item : batch.items) {
                const auto  status  = catalog->GetStatus(item.ticket);
                const auto *current = std::get_if<ResourceCatalogStatus>(&status);
                if (!current || current->state != RenderResourceState::UPLOAD_QUEUED || current->upload.completedBytes != item.first)
                    return false;
                const auto  tracked  = context->GetResourceState(TrackingResource(item.backing));
                const auto *snapshot = std::get_if<FrameResourceStateSnapshot>(&tracked);
                if (!snapshot || snapshot->accepted.state != item.initial.state || snapshot->accepted.submission != item.initial.submission)
                    return false;
                GraphBufferHandle bufferTarget;
                GraphImageHandle  imageTarget;
                for (uint32_t i = 0; i < item.copies.size(); ++i) {
                    const auto pass = item.pass + i;
                    if (pass >= executed.size() || !executed[pass])
                        return false;
                    GraphBufferHandle staging;
                    if (const auto *expected = std::get_if<BufferCopyPass>(&item.copies[i])) {
                        const auto *copy = std::get_if<BufferCopyPass>(&data.passes[pass]);
                        if (!copy || copy->source != expected->source || copy->destination != expected->destination ||
                            copy->sourceOffset != expected->sourceOffset || copy->destinationOffset != expected->destinationOffset ||
                            copy->size != expected->size || !copy->sideEffect || copy->destination.index >= data.buffers.size())
                            return false;
                        const auto &target  = data.buffers[copy->destination.index];
                        const auto &backing = std::get<ResourceBufferBacking>(item.backing);
                        if (target.buffer.Get() != backing.buffer.Get() || target.size != item.total || target.initialized ||
                            target.initial != std::get<CopyBufferState>(item.initial.state) || target.final != CopyBufferState::COMMON)
                            return false;
                        const auto  contents = ResolveInitializedBufferContents(target);
                        const auto *resolved = std::get_if<std::vector<BufferContentRange>>(&contents);
                        const auto  prefix   = item.first ? std::vector<BufferContentRange>{{0, item.first}} : std::vector<BufferContentRange>{};
                        if (!resolved || *resolved != prefix)
                            return false;
                        bufferTarget = copy->destination;
                        staging      = copy->source;
                    } else {
                        const auto &expectedImage = std::get<BufferImageCopyPass>(item.copies[i]);
                        const auto *copy          = std::get_if<BufferImageCopyPass>(&data.passes[pass]);
                        if (!copy || !Matches(*copy, expectedImage) || copy->image.index >= data.images.size())
                            return false;
                        const auto &target  = data.images[copy->image.index];
                        const auto &backing = std::get<ResourceImageBacking>(item.backing);
                        if (target.image.Get() != backing.image.Get() || !IsSameImageStorage(target.descriptor, backing.descriptor) ||
                            target.initialized || target.initial != std::get<ImageBoundaryState>(item.initial.state) ||
                            target.final != ImageLayout::GENERAL)
                            return false;
                        if (!i) {
                            const uint32_t                      texelBytes = backing.descriptor.format == PixelFormat::RGBA16_SFLOAT ? 8 : 4;
                            const auto                          contents   = ResolveInitializedImageContents(target);
                            const auto                         *resolved   = std::get_if<ResolvedImageContents>(&contents);
                            const std::vector<ImageContentSpan> prefix =
                                (item.first != 0u) ? std::vector<ImageContentSpan>{{.begin = 0, .end = item.first / texelBytes}}
                                                   : std::vector<ImageContentSpan>{};
                            if (!resolved || resolved->ranges != prefix)
                                return false;
                        }
                        imageTarget = copy->image;
                        staging     = copy->buffer;
                    }
                    if (staging.index >= data.buffers.size())
                        return false;
                    const auto &source = data.buffers[staging.index];
                    if (source.buffer.Get() != item.staging.Get() || !source.initialized || source.initial != CopyBufferState::HOST_WRITE ||
                        source.final != CopyBufferState::HOST_WRITE)
                        return false;
                }
                // A published version contains only the admitted writes, even when consumers share its upload graph.
                for (const auto pass : receipt->passes) {
                    if (pass >= item.pass && pass - item.pass < item.copies.size())
                        continue;
                    if (const auto *write = std::get_if<BufferCopyPass>(&data.passes[pass]); write && write->destination == bufferTarget)
                        return false;
                    if (const auto *write = std::get_if<BufferImageCopyPass>(&data.passes[pass]);
                        write && ((write->direction == ImageCopyDirection::IMAGE_TO_BUFFER && write->buffer == bufferTarget) ||
                                  (write->direction == ImageCopyDirection::BUFFER_TO_IMAGE && write->image == imageTarget)))
                        return false;
                    if (const auto *write = std::get_if<FrameRasterPass>(&data.passes[pass]);
                        (write != nullptr) && imageTarget != GraphImageHandle{} &&
                        (write->color == imageTarget || (write->depth && write->depth->image == imageTarget) ||
                         (write->colorResolve && write->colorResolve->image == imageTarget) ||
                         (write->depth && write->depth->resolve && write->depth->resolve->image == imageTarget))) {
                        return false;
                    }
                }
            }
            return true;
        }
    };

    ResourceUploader::ResourceUploader() : state(std::make_unique<State>())
    {
    }
    ResourceUploader::~ResourceUploader()
    {
        for (const auto &[id, batch] : state->batches) {
            for (const auto &item : batch.items) {
                if (!batch.submitted.serial)
                    state->catalog->RollbackUpload(item.ticket);
                else
                    state->catalog->Fail(item.ticket, {RendererErrorCode::NOT_READY, "Upload owner stopped before publication"});
            }
        }
        for (const auto &[sequence, resource] : state->resources)
            state->catalog->RestartUpload(resource.ticket);
    }
    std::variant<FrameError, std::unique_ptr<ResourceUploader>>
    ResourceUploader::Create(Device &device, DeviceFrameContext &context, ResourceCatalog &catalog, const ResourceUploadLimits &limits)
    {
        const auto identity = context.GetIdentity();
        if (!limits.bytesPerFrame || !limits.inflightBytes || !limits.createsPerFrame || !limits.pendingBatches || !limits.pendingResources ||
            !limits.pendingResourceBytes || !identity.contextId || identity.backend != device.GetAPI() ||
            identity.deviceEpoch != catalog.DeviceEpoch())
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid upload budgets or device epoch"};
        auto owner            = std::unique_ptr<ResourceUploader>(new ResourceUploader());
        owner->state->device  = &device;
        owner->state->context = &context;
        owner->state->catalog = &catalog;
        owner->state->limits  = limits;
        return owner;
    }
    ResourceUploadResult
    ResourceUploader::Prepare(const DeviceFrameLease &lease, FrameGraphBuilder &builder, std::span<const ResourceTicket> requests)
    {
        if (state->terminal)
            return *state->terminal;
        if (lease.context != state->context->GetIdentity() || !lease.generation)
            return FrameError{FrameErrorCode::WRONG_CONTEXT};
        if (state->batches.size() >= state->limits.pendingBatches)
            return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Upload batch capacity exceeded"};
        for (const auto &[id, batch] : state->batches)
            if (batch.lease == lease)
                return Invalid("Only one upload batch can own a lease");

        std::vector<ResourceTicket> sorted(requests.begin(), requests.end());
        std::stable_sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) { return a.sequence < b.sequence; });
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        for (const auto &ticket : sorted) {
            const auto status = state->catalog->GetStatus(ticket);
            if (std::holds_alternative<RendererError>(status))
                return Invalid("Upload request has a stale catalog ticket");
            const auto payload = state->catalog->BorrowPayload(ticket);
            if (payload && (payload->bytes.empty() || payload->bytes.size() % 4))
                return FrameError{FrameErrorCode::UNSUPPORTED, "Upload requires complete four-byte-aligned content"};
            if (payload && payload->metadata.image) {
                const auto &image = *payload->metadata.image;
                if (RenderImageByteSize(image) != payload->bytes.size() || image.arrayLayers > UINT16_MAX ||
                    uint64_t(image.mipLevels) * image.arrayLayers > 1024 || image.width > INT32_MAX / RenderImageTexelBytes(image.format) ||
                    image.height > INT32_MAX) {
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Image upload exceeds the supported complete subresource layout"};
                }
            }
        }
        state->PruneResources();
        ResourceUploadBatch result;
        State::Batch        batch;
        batch.lease         = lease;
        const auto rollback = [&] { state->Rollback(batch); };
        for (const auto &ticket : sorted) {
            const auto status  = std::get<ResourceCatalogStatus>(state->catalog->GetStatus(ticket));
            const auto payload = state->catalog->BorrowPayload(ticket);
            if (status.state != RenderResourceState::CPU_READY || !payload) {
                result.deferred.push_back({ticket, UploadDeferReason::NOT_READY});
                continue;
            }
            const auto total    = static_cast<uint64_t>(payload->bytes.size());
            const auto first    = status.upload.completedBytes;
            const bool image    = payload->metadata.image.has_value();
            const auto existing = state->resources.find(ticket.sequence);
            const bool create   = existing == state->resources.end();
            if (first >= total || (first && create)) {
                rollback();
                return Invalid("Upload progress has no matching resource backing");
            }
            const auto                       spent             = state->budgetLease == lease ? state->frameBytes : 0;
            const auto                       creates           = state->budgetLease == lease ? state->frameCreates : 0;
            const auto                       frameRemaining    = state->limits.bytesPerFrame - spent;
            const auto                       inflightRemaining = state->limits.inflightBytes - state->reserved - batch.bytes;
            const uint64_t                   minimum           = image ? RenderImageTexelBytes(payload->metadata.image->format) : 4;
            std::optional<UploadDeferReason> deferred;
            if (minimum > frameRemaining)
                deferred = UploadDeferReason::FRAME_BYTES;
            else if (create && creates >= state->limits.createsPerFrame)
                deferred = UploadDeferReason::CREATES;
            else if (minimum > inflightRemaining)
                deferred = UploadDeferReason::INFLIGHT;
            else if (create && state->resources.size() >= state->limits.pendingResources)
                deferred = UploadDeferReason::RESOURCE_COUNT;
            else if (create && total > state->limits.pendingResourceBytes - state->resourceBytes)
                deferred = UploadDeferReason::RESOURCE_BYTES;
            if (deferred) {
                result.deferred.push_back({ticket, *deferred});
                continue;
            }
            const auto capacity = state->context->AvailableFrameBufferBytes(lease, FrameBufferKind::STAGING, minimum);
            if (const auto *error = std::get_if<FrameError>(&capacity)) {
                rollback();
                return *error;
            }
            const auto available = std::get<uint64_t>(capacity);
            if (available < minimum) {
                result.deferred.push_back({ticket, UploadDeferReason::STAGING});
                continue;
            }
            auto                             size = std::min({total - first, frameRemaining, inflightRemaining, available}) & ~(minimum - 1);
            std::vector<BufferImageCopyPass> imageCopies;
            if (image) {
                imageCopies = PlanImageChunk(*payload->metadata.image, first, size, 1024 - builder.GetPassCount());
                if (!size) {
                    rollback();
                    return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Upload graph has no copy pass capacity"};
                }
            }
            auto staging = state->context->WriteFrameBuffer(lease, FrameBufferKind::STAGING,
                                                            std::as_bytes(std::span(payload->bytes).subspan(first, size)), minimum);
            if (const auto *error = std::get_if<FrameError>(&staging)) {
                if (error->code == FrameErrorCode::OUT_OF_MEMORY) {
                    result.deferred.push_back({ticket, UploadDeferReason::STAGING});
                    continue;
                }
                rollback();
                return *error;
            }
            if (state->budgetLease != lease) {
                state->budgetLease  = lease;
                state->frameBytes   = 0;
                state->frameCreates = 0;
            }
            state->frameBytes += size;
            if (create)
                ++state->frameCreates;
            const auto &slice  = std::get<FrameBufferSlice>(staging);
            auto        source = builder.Import(slice.buffer, CopyBufferState::HOST_WRITE, CopyBufferState::HOST_WRITE, true);
            if (const auto *error = std::get_if<FrameError>(&source)) {
                rollback();
                return *error;
            }
            State::Item item;
            item.ticket  = ticket;
            item.staging = slice.buffer;
            item.pass    = builder.GetPassCount();
            item.first   = first;
            item.bytes   = size;
            item.total   = total;
            if (!create) {
                item.backing = existing->second.backing;
            } else if (image) {
                const auto       &layout = *payload->metadata.image;
                Image::Descriptor desc;
                desc.extent      = {layout.width, layout.height, 1};
                desc.mipLevels   = layout.mipLevels;
                desc.arrayLayers = layout.arrayLayers;
                desc.format      = layout.format == RenderImageFormat::RGBA16_FLOAT ? PixelFormat::RGBA16_SFLOAT
                                   : layout.format == RenderImageFormat::RGBA8_SRGB ? PixelFormat::RGBA8_SRGB
                                                                                    : PixelFormat::RGBA8_UNORM;
                desc.usage       = ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::TRANSFER_DST | ImageUsageFlagBit::SAMPLED;
                ImagePtr target(state->device->CreateImage(desc));
                if (!target) {
                    rollback();
                    return FrameError{FrameErrorCode::COMPILE_FAILED, "GPU upload image creation failed"};
                }
                item.backing = ResourceImageBacking{std::move(target), desc};
            } else {
                Buffer::Descriptor desc;
                desc.size = total;
                desc.usage =
                    BufferUsageFlagBit::TRANSFER_SRC | BufferUsageFlagBit::TRANSFER_DST | BufferUsageFlagBit::VERTEX | BufferUsageFlagBit::INDEX;
                BufferPtr target(state->device->CreateBuffer(desc));
                if (!target) {
                    rollback();
                    return FrameError{FrameErrorCode::COMPILE_FAILED, "GPU upload buffer creation failed"};
                }
                item.backing = ResourceBufferBacking{std::move(target)};
            }
            if (create) {
                const FrameResourceBoundaryState initial =
                    image ? FrameResourceBoundaryState{ImageLayout::UNDEFINED} : FrameResourceBoundaryState{CopyBufferState::COMMON};
                auto tracked = state->context->TrackOwnedResource(TrackingResource(item.backing), initial);
                if (const auto *error = std::get_if<FrameError>(&tracked)) {
                    if (error->code == FrameErrorCode::OUT_OF_MEMORY) {
                        result.deferred.push_back({ticket, UploadDeferReason::STATE_TRACKING});
                        continue;
                    }
                    rollback();
                    return *error;
                }
                const auto &owner = std::get<std::shared_ptr<const FrameResourceTracking>>(tracked);
                std::visit([&](auto &backing) { backing.tracking = owner; }, item.backing);
            }
            const auto tracked = state->context->GetResourceState(TrackingResource(item.backing));
            if (const auto *error = std::get_if<FrameError>(&tracked)) {
                rollback();
                return *error;
            }
            item.initial = std::get<FrameResourceStateSnapshot>(tracked).accepted;
            if (const auto *backing = std::get_if<ResourceImageBacking>(&item.backing)) {
                auto destination = builder.Import(ImportImagePrefix(*backing, first, std::get<ImageBoundaryState>(item.initial.state)));
                if (const auto *error = std::get_if<FrameError>(&destination)) {
                    rollback();
                    return *error;
                }
                item.copies.reserve(imageCopies.size());
                for (auto &copy : imageCopies) {
                    copy.buffer = std::get<GraphBufferHandle>(source);
                    copy.image  = std::get<GraphImageHandle>(destination);
                    copy.region.bufferOffset += slice.offset;
                    item.copies.emplace_back(std::move(copy));
                }
            } else {
                const auto      &buffer = std::get<ResourceBufferBacking>(item.backing);
                FrameGraphBuffer target{buffer.buffer, total, std::get<CopyBufferState>(item.initial.state), CopyBufferState::COMMON, false};
                if (first)
                    target.initializedRanges.push_back({0, first});
                auto destination = builder.Import(std::move(target));
                if (const auto *error = std::get_if<FrameError>(&destination)) {
                    rollback();
                    return *error;
                }
                item.copies.emplace_back(
                    BufferCopyPass{std::get<GraphBufferHandle>(source), std::get<GraphBufferHandle>(destination), slice.offset, first, size, true});
            }
            for (const auto &copy : item.copies) {
                const auto copied = std::visit([&](const auto &value) { return builder.Copy(value); }, copy);
                if (const auto *error = std::get_if<FrameError>(&copied)) {
                    rollback();
                    return *error;
                }
            }
            if (!state->catalog->QueueUpload(ticket)) {
                rollback();
                return Invalid("Upload candidate changed during coordinator admission");
            }
            if (create) {
                state->resources.emplace(ticket.sequence, State::PendingResource{ticket, item.backing, total, true});
                state->resourceBytes += total;
            } else {
                existing->second.pending = true;
            }
            batch.bytes += size;
            batch.items.push_back(std::move(item));
            result.admitted.push_back(ticket);
        }
        if (!batch.items.empty()) {
            result.id = Uuid::Create();
            if (!result.id || state->batches.contains(result.id)) {
                rollback();
                return Invalid("Could not allocate upload batch identity");
            }
            batch.graph = builder.GetId();
            state->reserved += batch.bytes;
            state->batches.emplace(result.id, std::move(batch));
        }
        return result;
    }
    std::variant<FrameError, GraphBufferHandle, GraphImageHandle>
    ResourceUploader::ResolveInGraph(const Uuid &id, const ResourceTicket &ticket, const FrameGraphBuilder &builder) const
    {
        const auto batch = state->batches.find(id);
        if (batch == state->batches.end() || batch->second.submitted.serial || batch->second.graph != builder.GetId())
            return Invalid("InGraph resource belongs to a different or retired graph");
        for (const auto &item : batch->second.items)
            if (item.ticket == ticket) {
                if (item.first + item.bytes != item.total)
                    return Invalid("InGraph access requires the complete resource version");
                if (const auto *copy = std::get_if<BufferCopyPass>(&item.copies.front()))
                    return copy->destination;
                return std::get<BufferImageCopyPass>(item.copies.front()).image;
            }
        return Invalid("Resource was not admitted to this graph");
    }
    std::shared_ptr<const ResourceBacking>
    ResourceUploader::ResolveBackingInGraph(const Uuid &id, const ResourceTicket &ticket, const FrameGraphBuilder &builder) const
    {
        if (std::holds_alternative<FrameError>(ResolveInGraph(id, ticket, builder)))
            return {};
        for (const auto &item : state->batches.at(id).items)
            if (item.ticket == ticket)
                return std::make_shared<const ResourceBacking>(item.backing);
        return {};
    }
    FrameSubmitResult ResourceUploader::Submit(const Uuid &id, std::unique_ptr<RecordedFrame> &recording, const FrameSubmissionSync &sync)
    {
        const auto found = state->batches.find(id);
        if (found == state->batches.end() || found->second.submitted.serial || !recording || !state->Verify(found->second, *recording))
            return SubmissionRejected{Invalid("Recording does not prove this upload batch executed unchanged")};
        auto &batch  = found->second;
        auto  result = state->context->Submit(recording, sync);
        if (const auto *accepted = std::get_if<SubmissionAccepted>(&result)) {
            batch.submitted = accepted->token;
            for (const auto &item : batch.items)
                state->catalog->AcceptUpload(item.ticket, accepted->token, item.bytes);
        } else if (const auto *uncertain = std::get_if<SubmissionIndeterminate>(&result)) {
            batch.submitted = uncertain->token;
            batch.uncertain = true;
            state->terminal = uncertain->error;
            for (const auto &item : batch.items)
                state->catalog->Fail(item.ticket, ResourceFailure(uncertain->error));
        } else if (const auto &error = std::get<SubmissionRejected>(result).error;
                   error.code == FrameErrorCode::DEVICE_LOST || error.cause.code == GpuErrorCode::DEVICE_LOST) {
            state->terminal = error;
            for (const auto &item : batch.items)
                state->catalog->Fail(item.ticket, ResourceFailure(error));
        }
        return result;
    }
    FrameOperationResult ResourceUploader::Discard(const Uuid &id)
    {
        const auto found = state->batches.find(id);
        if (found == state->batches.end() || found->second.submitted.serial)
            return Invalid("Only an unsubmitted upload batch can be discarded");
        state->Rollback(found->second);
        state->reserved -= found->second.bytes;
        state->batches.erase(found);
        return FrameOperationComplete{};
    }
    size_t ResourceUploader::PollCompleted(std::span<FrameCompletion> completions)
    {
        const auto count = state->context->PollCompleted(completions);
        for (const auto &completion : completions.first(count))
            if (completion.status != FrameCompletionStatus::COMPLETE)
                state->terminal = completion.error;
        for (const auto &completion : completions.first(count)) {
            for (auto it = state->batches.begin(); it != state->batches.end(); ++it) {
                auto &batch = it->second;
                if (!batch.submitted.serial || batch.submitted != completion.token)
                    continue;
                for (const auto &item : batch.items) {
                    if (completion.status == FrameCompletionStatus::COMPLETE) {
                        const auto tracked = state->context->GetResourceState(TrackingResource(item.backing));
                        if (const auto *error = std::get_if<FrameError>(&tracked); error && !state->terminal)
                            state->terminal = *error;
                    }
                    if (state->terminal) {
                        state->catalog->Fail(item.ticket, ResourceFailure(*state->terminal));
                        if (completion.status != FrameCompletionStatus::COMPLETE)
                            continue;
                    }
                    if (auto resource = state->resources.find(item.ticket.sequence); resource != state->resources.end())
                        resource->second.pending = false;
                    if (state->terminal || batch.uncertain || !state->catalog->CompleteUpload(item.ticket, completion.token) ||
                        item.first + item.bytes != item.total)
                        continue;
                    auto value = item.backing;
                    std::visit([&](auto &backing) { backing.initializedBy = completion.token; }, value);
                    auto backing = std::make_shared<const ResourceBacking>(std::move(value));
                    if (auto error = state->catalog->AdoptResident(item.ticket, item.ticket.resource, std::move(backing))) {
                        state->catalog->Fail(item.ticket, std::move(*error));
                        state->terminal = Invalid("Completed upload could not be published");
                    }
                }
                if (completion.status == FrameCompletionStatus::COMPLETE) {
                    state->reserved -= batch.bytes;
                    state->batches.erase(it);
                    state->PruneResources();
                }
                break;
            }
        }
        return count;
    }
    uint64_t ResourceUploader::InflightBytes() const noexcept
    {
        return state->reserved;
    }
    uint64_t ResourceUploader::PendingResourceBytes() const noexcept
    {
        return state->resourceBytes;
    }
} // namespace sky::aurora
