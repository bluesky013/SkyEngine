//
// Created on 2026/10/17.
//

#include "RendererRuntime.h"

#include "frame/CpuRenderCoordinator.h"
#include "frame/FrameScheduler.h"
#include "frame/ViewportMapping.h"
#include "graph/FrameComposer.h"
#include "graph/FrameFeatureRegistry.h"
#include "graph/FrameGraphBuilder.h"
#include "graph/PreparedRaster.h"
#include "prepare/PreparedScene.h"
#include "resource/ResourceUploader.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <aurora/rhi/Instance.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <tuple>
#include <type_traits>

namespace sky::aurora {
    namespace {
        using ResourceKey = std::tuple<Uuid, uint64_t, uint64_t, uint64_t>;
        ResourceKey Key(const RenderResourceKey &key)
        {
            return {key.product, key.usageVariant, key.deviceEpoch, key.revision};
        }
        RendererError Error(const FrameError &error)
        {
            const auto code = error.code == FrameErrorCode::DEVICE_LOST || error.cause.code == GpuErrorCode::DEVICE_LOST
                                  ? RendererErrorCode::DEVICE_LOST
                              : error.code == FrameErrorCode::OUT_OF_MEMORY ? RendererErrorCode::OUT_OF_MEMORY
                                                                            : RendererErrorCode::INTERNAL_ERROR;
            return {code, (error.message.empty() ? "Renderer frame operation failed" : error.message) + " (frame=" +
                              std::to_string(static_cast<uint32_t>(error.code)) + ", native=" + std::to_string(error.cause.nativeCode) + ")"};
        }
        struct RuntimeEvents {
            void Push(RendererEvent event)
            {
                std::lock_guard lock(mutex);
                if (const auto *resource = std::get_if<ResourceStateChanged>(&event)) {
                    for (auto &pending : events)
                        if (auto *old = std::get_if<ResourceStateChanged>(&pending); old && old->resource == resource->resource) {
                            pending = std::move(event);
                            return;
                        }
                }
                events.push_back(std::move(event));
            }
            std::mutex                mutex;
            std::deque<RendererEvent> events;
            RendererInfo              info;
            std::atomic_bool          failed = false;
        };
        struct RuntimeOutput;
        struct FrameBuildContext {
            FrameBuildInputs                                   inputs;
            FrozenResourceCatalog                              resources;
            std::vector<OutputSnapshot>                        outputs;
            std::vector<ViewportId>                            readbacks;
            std::map<uint32_t, std::shared_ptr<RuntimeOutput>> outputVersions;
            std::vector<PreparedMeshLayout>                    meshes;
            std::vector<PreparedMaterialLayout>                materials;
            std::vector<FrameFeatureUse>                       features;
        };
        struct PendingReadback {
            ViewportId  viewport;
            Extent2D    extent;
            PixelFormat format;
            BufferPtr   buffer;
        };
        struct SubmittedFrame {
            uint64_t                     sequence = 0;
            std::vector<PendingReadback> readbacks;
        };
        struct EncodingFrame {
            uint64_t                                    sequence = 0;
            DeviceFrameLease                            lease;
            Uuid                                        upload;
            EncodingTicket                              ticket;
            std::vector<std::shared_ptr<RuntimeOutput>> offscreen;
            std::vector<PendingReadback>                readbacks;
        };
        struct RuntimeOutput {
            CpuViewportState  cpu;
            ImagePtr          image;
            Image::Descriptor descriptor;
            bool              initialized = false;
        };
        struct RetirementRecord {
            CpuRetirement id;
            uint64_t      sequence = 0;
        };

        class RendererRuntime final : public CpuCoordinatorRuntime {
        public:
            RendererRuntime(RendererInitInfo value, Instance &instance, std::shared_ptr<RuntimeEvents> events)
                : init(value), instance(instance), events(std::move(events))
            {
            }
            std::optional<RendererError> Initialize() override
            {
                if (init.backend == API::METAL)
                    return RendererError{RendererErrorCode::UNSUPPORTED, "Metal renderer frame protocol is not available"};
                auto acquired = instance.Acquire({"Aurora Renderer", "SkyEngine", init.enableValidation, init.backend});
                if (const auto *error = std::get_if<GpuError>(&acquired))
                    return RendererError{error->code == GpuErrorCode::INVALID_ARGUMENT ? RendererErrorCode::NOT_READY
                                                                                       : RendererErrorCode::UNSUPPORTED,
                                         "Renderer requires an available, exclusive backend device"};
                session = std::move(std::get<std::unique_ptr<Instance::DeviceSession>>(acquired));
                device  = session->GetDevice();
                DeviceFrameContextInitInfo info;
                info.inflightNum              = init.inflightFrames;
                info.parallelNum              = init.encodeWorkers;
                info.deviceEpoch              = session->GetEpoch();
                info.allocations.stagingBytes = init.maxUploadBytesPerFrame;
                info.vulkanCompiler =
                    init.vulkanMode == RendererVulkanMode::RENDER_PASS ? VulkanFrameGraphMode::RENDER_PASS : VulkanFrameGraphMode::DYNAMIC_RENDERING;
                context.reset(device->CreateFrameContext(info));
                if (!context || context->GetIdentity().deviceEpoch != info.deviceEpoch)
                    return RendererError{RendererErrorCode::UNSUPPORTED, "Backend could not initialize its frame context"};
                auto created = ResourceCatalog::Create({info.deviceEpoch, init.maxPendingResourceBytes});
                if (const auto *error = std::get_if<RendererError>(&created))
                    return *error;
                catalog = std::move(std::get<std::unique_ptr<ResourceCatalog>>(created));
                ResourceUploadLimits limits;
                limits.bytesPerFrame        = init.maxUploadBytesPerFrame;
                limits.inflightBytes        = init.maxUploadBytesPerFrame * init.inflightFrames;
                limits.pendingBatches       = init.inflightFrames;
                limits.pendingResourceBytes = init.maxPendingResourceBytes;
                auto uploaded               = ResourceUploader::Create(*device, *context, *catalog, limits);
                if (const auto *error = std::get_if<FrameError>(&uploaded))
                    return Error(*error);
                uploader     = std::move(std::get<std::unique_ptr<ResourceUploader>>(uploaded));
                scheduler    = std::make_unique<FrameScheduler>(*device, *context);
                events->info = {device->GetAPI(), info.deviceEpoch};
                return {};
            }
            std::optional<RendererError> Validate(const FramePacket &packet) const override
            {
                if (auto error = features.Validate(packet))
                    return error;
                uint64_t   bytes    = 0;
                const auto validKey = [&](const RenderResourceKey &key) {
                    return key.product && key.revision && key.deviceEpoch == events->info.deviceEpoch;
                };
                if (packet.releasedResources.size() > 4096 ||
                    !std::all_of(packet.releasedResources.begin(), packet.releasedResources.end(), validKey))
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid released resource versions"};
                for (const auto &payload : packet.extensions)
                    if (!std::all_of(payload.resources.begin(), payload.resources.end(), validKey))
                        return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid feature resource versions"};
                if (packet.commands.size() > 65536 || packet.readbacks.size() > 16)
                    return RendererError{RendererErrorCode::INVALID_PACKET, "Packet command budget exceeded"};
                for (const auto &command : packet.commands) {
                    const auto *ready = std::get_if<ResourceCpuReady>(&command);
                    if (!ready)
                        continue;
                    const auto &payload = ready->payload;
                    if (payload.resource.deviceEpoch != events->info.deviceEpoch || !payload.resource.product || !payload.resource.revision ||
                        payload.format.Empty() || payload.bytes.empty() || payload.bytes.size() % 4 ||
                        !IsValidRenderResourceMetadata(payload.metadata) || (payload.mesh && payload.material) ||
                        (payload.metadata.image && (payload.mesh || payload.material)))
                        return RendererError{RendererErrorCode::INVALID_PACKET, "Invalid owned renderer resource payload"};
                    const auto payloadBytes = RenderResourceCpuBytes(payload);
                    if (payloadBytes > init.maxPendingResourceBytes - bytes)
                        return RendererError{RendererErrorCode::OUT_OF_MEMORY, "Packet resource budget exceeded"};
                    bytes += payloadBytes;
                    if (payload.metadata.image) {
                        const auto &image = *payload.metadata.image;
                        if (RenderImageByteSize(image) != payload.bytes.size() || image.arrayLayers > UINT16_MAX ||
                            uint64_t(image.mipLevels) * image.arrayLayers > 1024 || image.width > INT32_MAX / 4 || image.height > INT32_MAX)
                            return RendererError{RendererErrorCode::INVALID_PACKET, "Image payload exceeds supported upload layout"};
                    }
                    if (payload.mesh && (payload.mesh->streams.empty() || payload.mesh->attributes.empty() ||
                                         payload.mesh->lods.size() != payload.metadata.lodDistances.size() + 1))
                        return RendererError{RendererErrorCode::INVALID_PACKET, "Mesh layout and LOD metadata disagree"};
                    if (payload.mesh) {
                        const auto validRange = [&](uint64_t offset, uint64_t size) {
                            return size && offset <= payload.bytes.size() && size <= payload.bytes.size() - offset;
                        };
                        for (const auto &stream : payload.mesh->streams)
                            if (!validRange(stream.offset, stream.size) || !stream.stride || stream.size < stream.stride)
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Mesh stream exceeds its owned bytes"};
                        for (const auto &lod : payload.mesh->lods)
                            if (!validRange(lod.offset, lod.size))
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Mesh LOD exceeds its owned bytes"};
                    }
                    if (payload.material && (payload.material->backend != device->GetAPI() || payload.material->vertexCode.empty() ||
                                             payload.material->fragmentCode.empty() || payload.material->vertexCode.size() > UINT32_MAX ||
                                             payload.material->fragmentCode.size() > UINT32_MAX))
                        return RendererError{RendererErrorCode::INVALID_PACKET, "Material requires code for the selected backend"};
                    if (payload.material) {
                        const auto         &material = *payload.material;
                        std::array<bool, 3> resources{}, blocks{};
                        if (material.vertexEntry.empty() || material.fragmentEntry.empty() || !material.reflection.pushConstants.empty())
                            return RendererError{RendererErrorCode::INVALID_PACKET, "Material requires the prepared raster uniform layout"};
                        for (const auto &resource : material.reflection.resources) {
                            const auto type = resource.set == 1 ? ShaderResourceType::UNIFORM_BUFFER : ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                            if (resource.set >= 3 || resource.binding || resource.count != 1 || resource.descriptorArray || resource.type != type ||
                                resources[resource.set])
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Unsupported prepared material resource layout"};
                            resources[resource.set] = true;
                        }
                        for (const auto &block : material.reflection.blocks) {
                            const uint64_t expected = block.set == 0 ? 80 : block.set == 2 ? 64 : payload.bytes.size();
                            if (block.set >= 3 || block.binding || block.size != expected || blocks[block.set])
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Prepared material uniform size mismatch"};
                            blocks[block.set] = true;
                        }
                        if (resources != std::array{true, true, true} || blocks != resources)
                            return RendererError{RendererErrorCode::INVALID_PACKET, "Prepared material requires View, Material and Object uniforms"};
                    }
                }
                return {};
            }
            std::optional<RendererError> Applied(CpuSceneStore &store, FramePacket &packet, const PacketApplyResult &applied) override
            {
                features.Apply(packet);
                pendingExtensions = std::move(packet.extensions);
                events->Push(PacketApplied{packet.sequence});
                requestedReadbacks = packet.readbacks;
                for (const auto changed : applied.changedScenes)
                    std::erase(pausedScenes, changed);
                for (const auto &retired : applied.retirements) {
                    retirements.push_back({retired, packet.sequence});
                    if (const auto *id = std::get_if<ViewportId>(&retired)) {
                        failedOutputs.erase(id->index);
                        const auto found = outputs.find(id->index);
                        if (found != outputs.end() && found->second->cpu.id == *id) {
                            if (found->second->cpu.desc.kind == ViewportKind::NATIVE_WINDOW) {
                                closing.insert({id->index, id->generation});
                                const auto closed = scheduler->RequestClose(*id);
                                if (const auto *error = std::get_if<FrameError>(&closed))
                                    terminal = Error(*error);
                            }
                            outputs.erase(found);
                        }
                    }
                }
                for (size_t index = 0; index < packet.commands.size(); ++index) {
                    if (std::any_of(applied.skipped.begin(), applied.skipped.end(),
                                    [&](const auto &skip) { return skip.source == CommandSource::PACKET && skip.commandIndex == index; }))
                        continue;
                    std::visit(
                        [&](const auto &command) {
                            using T = std::decay_t<decltype(command)>;
                            if constexpr (std::is_same_v<T, CreateViewport> || std::is_same_v<T, ResizeViewport>) {
                                if (const auto *viewport = store.FindViewport(command.id))
                                    UpdateOutput(*viewport);
                            } else if constexpr (std::is_same_v<T, CreateView> || std::is_same_v<T, UpdateView>) {
                                if (const auto *view = store.FindView(command.id))
                                    std::erase(pausedScenes, view->desc.scene);
                            }
                        },
                        packet.commands[index]);
                }
                for (const auto id : store.SelectPrepareInputs().scenes) {
                    const auto *scene = store.FindScene(id);
                    for (const auto &[index, entity] : scene->entities) {
                        const auto *mesh     = scene->registry.Get<RenderMeshBinding>(entity.entity);
                        const auto *material = scene->registry.Get<RenderMaterialBinding>(entity.entity);
                        for (const auto slot : {ResourceBindingSlot::MESH, ResourceBindingSlot::MATERIAL}) {
                            const auto &binding = slot == ResourceBindingSlot::MESH ? mesh->binding : material->binding;
                            if (!binding.resource.product)
                                continue;
                            const auto requested = catalog->Request(store, {{id, entity.id}, slot, binding.bindingRevision, packet.sequence});
                            if (const auto *request = std::get_if<ResourceRequest>(&requested))
                                tickets[Key(request->ticket.resource)] = request->ticket;
                            else
                                events->Push(
                                    ResourceStateChanged{binding.resource, RenderResourceState::FAILED, 0, std::get<RendererError>(requested)});
                        }
                    }
                }
                for (auto &command : packet.commands) {
                    auto *ready = std::get_if<ResourceCpuReady>(&command);
                    if (!ready)
                        continue;
                    auto      &payload   = ready->payload;
                    const auto key       = payload.resource;
                    const auto requested = catalog->Request(key, true);
                    if (const auto *error = std::get_if<RendererError>(&requested)) {
                        events->Push(ResourceStateChanged{key, RenderResourceState::FAILED, 0, *error});
                        continue;
                    }
                    const auto request = std::get<ResourceRequest>(requested);
                    tickets[Key(key)]  = request.ticket;
                    if (!request.needsCpuData)
                        continue;
                    if (auto error = CacheRaster(payload)) {
                        catalog->Fail(request.ticket, *error);
                    } else if (auto error = catalog->Supply(request.ticket, payload)) {
                        catalog->Fail(request.ticket, *error);
                    }
                }
                std::erase_if(pausedScenes, [&](SceneId id) { return !store.FindScene(id); });
                std::vector<RenderResourceKey> external;
                for (const auto &payload : pendingExtensions)
                    external.insert(external.end(), payload.resources.begin(), payload.resources.end());
                frozenResources = catalog->Freeze(store, true, external);
                for (const auto &key : packet.releasedResources)
                    catalog->Release(key);
                PublishResources();
                if (catalog->Prune(store))
                    PublishResources();
                snapshots.clear();
                for (const auto view : store.SelectPrepareInputs().views) {
                    const auto *viewport = store.FindViewport(store.FindView(view)->desc.output);
                    if (!outputs.contains(viewport->id.index))
                        UpdateOutput(*viewport);
                }
                for (const auto &[index, output] : outputs) {
                    if (output->cpu.desc.kind != ViewportKind::NATIVE_WINDOW)
                        continue;
                    auto queried = scheduler->BeginViewport(output->cpu.id);
                    if (const auto *snapshot = std::get_if<OutputSnapshot>(&queried)) {
                        failedOutputs.erase(output->cpu.id.index);
                        snapshots.push_back(*snapshot);
                        store.SetViewportSurface(output->cpu.id, {ViewportMapping{snapshot->extent, snapshot->transform}.LogicalExtent(),
                                                                  snapshot->format, snapshot->transform, snapshot->revision});
                    } else {
                        store.SetViewportSurface(output->cpu.id, {});
                        OutputError(std::get<OutputProgress>(queried));
                    }
                }
                return terminal;
            }
            CpuPrepareBuildResult Build(const FrozenCpuFrame &frame) override
            {
                for (const auto &input : frame.Views()) {
                    const auto clip = input.view.desc.projection * input.view.desc.view;
                    if (!std::all_of(std::begin(clip.v), std::end(clip.v), [](float value) { return std::isfinite(value); })) {
                        const auto id = input.view.desc.scene;
                        if (std::find(pausedScenes.begin(), pausedScenes.end(), id) == pausedScenes.end()) {
                            pausedScenes.push_back(id);
                            events->Push(
                                ScenePrepareFailed{id, frame.Sequence(), {RendererErrorCode::INVALID_ARGUMENT, "View projection overflows"}});
                        }
                    }
                }
                auto built = BuildPreparedSceneProgram(frame, frozenResources.cpu, 1024, pausedScenes);
                if (const auto *error = std::get_if<RendererError>(&built))
                    return *error;
                program                   = std::move(std::get<PreparedSceneProgram>(built));
                program->frame.extensions = std::move(pendingExtensions);
                return std::move(program->work);
            }
            bool RecoverPrepareFailure(const CpuPacketCompleted &result) override
            {
                if (!program || result.preparation.cancelled)
                    return false;
                bool isolated = false;
                for (const auto &node : result.preparation.nodes) {
                    if (node.state != PrepareNodeState::FAILED)
                        continue;
                    SceneId id;
                    for (const auto &[system, scene] : program->sceneSystems)
                        if (system == node.system)
                            id = scene;
                    for (size_t i = 0; i < program->viewSystems.size(); ++i)
                        if (program->viewSystems[i] == node.system)
                            id = program->frame.views[i].view.scene;
                    if (!id.IsValid())
                        return false;
                    if (std::find(pausedScenes.begin(), pausedScenes.end(), id) == pausedScenes.end())
                        pausedScenes.push_back(id);
                    events->Push(ScenePrepareFailed{id, result.application.sequence, {RendererErrorCode::INTERNAL_ERROR, node.diagnostic}});
                    isolated = true;
                }
                return isolated;
            }
            std::optional<RendererError> Prepared(CpuSceneStore &store, const CpuPacketCompleted &result) override
            {
                if (!program || !result.preparation.Succeeded()) {
                    program.reset();
                    frozenResources = {};
                    return {};
                }
                auto collected = CollectPreparedScene(*program, result.preparation);
                program.reset();
                if (const auto *error = std::get_if<RendererError>(&collected))
                    return *error;
                while (pending.size() >= init.maxPendingPackets) {
                    if (auto error = Pump(store)) {
                        terminal = std::move(error);
                        events->Push(RendererFailure{result.application.sequence, *terminal});
                        frozenResources = {};
                        return {};
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                FrameBuildContext frame;
                frame.inputs.prepared = std::move(std::get<std::shared_ptr<const PreparedFrame>>(collected));
                frame.resources       = std::move(frozenResources);
                frame.outputs         = std::move(snapshots);
                frame.readbacks       = std::move(requestedReadbacks);
                frame.outputVersions  = outputs;
                frame.features        = features.Snapshot();
                for (const auto &pin : frame.resources.pins) {
                    const auto key = Key(pin->cpu->resource);
                    if (const auto found = meshes.find(key); found != meshes.end())
                        frame.meshes.push_back(found->second);
                    if (const auto found = materials.find(key); found != materials.end())
                        frame.materials.push_back(found->second);
                }
                pending.push_back(std::move(frame));
                // GPU work owns the frame's final result; the CPU acknowledgement must not duplicate it.
                if (auto error = Pump(store))
                    terminal = std::move(error);
                return {};
            }
            std::optional<RendererError> Pump(CpuSceneStore &store) override
            {
                if (!uploader || !scheduler)
                    return {};
                std::array<FrameCompletion, 16> completions;
                const auto                      completed = uploader->PollCompleted(completions);
                scheduler->OnCompleted(std::span(completions).first(completed));
                for (const auto &completion : std::span(completions).first(completed)) {
                    const auto found = submissions.find(completion.token.serial);
                    if (completion.status != FrameCompletionStatus::COMPLETE) {
                        terminal = Error(completion.error);
                        if (found != submissions.end() && found->second.sequence)
                            events->Push(RendererFailure{found->second.sequence, *terminal});
                    } else if (found != submissions.end() && found->second.sequence) {
                        bool readbackFailed = false;
                        for (const auto &readback : found->second.readbacks) {
                            const auto *mapped = readback.buffer->Map();
                            if (!mapped) {
                                readbackFailed = true;
                                continue;
                            }
                            ViewportReadback result{found->second.sequence, readback.viewport, readback.extent, readback.format, {}};
                            result.bytes.assign(mapped, mapped + readback.buffer->GetSize());
                            readback.buffer->UnMap();
                            events->Push(std::move(result));
                        }
                        if (readbackFailed)
                            events->Push(RendererFailure{found->second.sequence, {RendererErrorCode::INTERNAL_ERROR, "Readback mapping failed"}});
                        else
                            events->Push(FrameCompleted{found->second.sequence, completion.token.serial});
                    }
                    if (found != submissions.end())
                        submissions.erase(found);
                }
                std::array<OutputProgress, 64> progress;
                const auto                     count = scheduler->Pump(progress);
                for (const auto &output : std::span(progress).first(count)) {
                    if (output.kind == OutputProgressKind::CLOSED)
                        closed.insert({output.viewport.index, output.viewport.generation});
                    OutputError(output);
                }
                for (auto it = encoding.begin(); it != encoding.end();) {
                    auto result = context->TryCollect(it->ticket);
                    if (std::holds_alternative<FramePending>(result)) {
                        ++it;
                        continue;
                    }
                    if (auto *recording = std::get_if<std::unique_ptr<RecordedFrame>>(&result)) {
                        auto submitted = scheduler->Submit(*recording, it->upload ? uploader.get() : nullptr, it->upload);
                        if (const auto *accepted = std::get_if<SubmissionAccepted>(&submitted)) {
                            submissions[accepted->token.serial] = {it->sequence, std::move(it->readbacks)};
                            for (const auto &output : it->offscreen)
                                output->initialized = true;
                        } else if (const auto *uncertain = std::get_if<SubmissionIndeterminate>(&submitted)) {
                            terminal = Error(uncertain->error);
                            events->Push(RendererFailure{it->sequence, *terminal});
                        } else {
                            const auto error = std::get<SubmissionRejected>(submitted).error;
                            recording->reset();
                            Discard(*it, error);
                        }
                    } else {
                        const auto *failed = std::get_if<EncodingFailed>(&result);
                        const auto *error  = std::get_if<FrameError>(&result);
                        Discard(*it, failed ? failed->error : error ? *error : FrameError{FrameErrorCode::ENCODE_FAILED});
                    }
                    it = encoding.erase(it);
                }
                PublishResources();
                if (catalog->Prune(store))
                    PublishResources();
                if (terminal)
                    return terminal;
                if (encoding.empty()) {
                    if (!pending.empty()) {
                        auto begun = context->TryBeginSlot();
                        if (const auto *error = std::get_if<FrameError>(&begun))
                            return Error(*error);
                        if (const auto *lease = std::get_if<DeviceFrameLease>(&begun)) {
                            auto frame = std::move(pending.front());
                            pending.pop_front();
                            frame.inputs.lease = *lease;
                            BuildFrame(std::move(frame));
                        }
                    } else if (!stopping && HasUploads()) {
                        FrameBuildContext frame;
                        frame.inputs.prepared = std::make_shared<const PreparedFrame>();
                        pending.push_back(std::move(frame));
                    }
                }
                Retire(store);
                for (const auto id : features.CollectRetired())
                    events->Push(RenderFeatureRetired{id});
                return terminal;
            }
            std::optional<RendererError> Shutdown(CpuSceneStore &store) override
            {
                stopping            = true;
                bool outputsClosing = false;
                while (scheduler && !terminal) {
                    if (!outputsClosing && pending.empty() && encoding.empty() && submissions.empty()) {
                        scheduler->RequestShutdown();
                        outputsClosing = true;
                    }
                    if (outputsClosing && scheduler->Empty())
                        break;
                    if (auto error = Pump(store))
                        terminal = std::move(error);
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                if (scheduler && !outputsClosing)
                    scheduler->RequestShutdown();
                if (terminal) {
                    events->failed = true;
                    for (const auto &frame : pending)
                        if (frame.inputs.prepared->packetSequence)
                            events->Push(RendererFailure{frame.inputs.prepared->packetSequence, *terminal});
                    for (const auto &frame : encoding) {
                        if (frame.sequence)
                            events->Push(RendererFailure{frame.sequence, *terminal});
                        context->Cancel(frame.ticket);
                    }
                    for (const auto &[serial, frame] : submissions)
                        if (frame.sequence)
                            events->Push(RendererFailure{frame.sequence, *terminal});
                }
                pending.clear();
                encoding.clear();
                submissions.clear();
                frozenResources = {};
                program.reset();
                outputs.clear();
                materials.clear();
                meshes.clear();
                uploader.reset();
                if (catalog) {
                    for (const auto &[key, ticket] : tickets) {
                        const auto status = catalog->GetStatus(ticket);
                        if (const auto *value = std::get_if<ResourceCatalogStatus>(&status); value && value->state != RenderResourceState::RESIDENT &&
                                                                                             value->state != RenderResourceState::FAILED &&
                                                                                             value->state != RenderResourceState::CANCELLED)
                            catalog->Fail(ticket, terminal.value_or(
                                                      RendererError{RendererErrorCode::NOT_READY, "Renderer stopped before resource completion"}));
                    }
                    PublishResources();
                }
                catalog.reset();
                scheduler.reset();
                context.reset();
                features.Shutdown();
                for (const auto id : features.CollectRetired())
                    events->Push(RenderFeatureRetired{id});
                session.reset();
                device = nullptr;
                for (const auto &retired : retirements)
                    Acknowledge(store, retired.id);
                retirements.clear();
                return terminal;
            }

        private:
            void OutputError(const OutputProgress &progress)
            {
                if (progress.kind != OutputProgressKind::FAILED)
                    return;
                RendererError error{progress.error.code == GpuErrorCode::DEVICE_LOST ? RendererErrorCode::DEVICE_LOST : RendererErrorCode::NOT_READY,
                                    "Viewport is unavailable (native=" + std::to_string(progress.error.nativeCode) + ")"};
                const auto reported = failedOutputs.find(progress.viewport.index);
                if (reported == failedOutputs.end() || reported->second != progress.disposition) {
                    events->Push(ViewportFailed{progress.viewport, error, progress.disposition == OutputFailureDisposition::REPLACE});
                    failedOutputs[progress.viewport.index] = progress.disposition;
                }
                if (error.code == RendererErrorCode::DEVICE_LOST || progress.disposition == OutputFailureDisposition::SHUTDOWN)
                    terminal = error;
            }
            void UpdateOutput(const CpuViewportState &viewport)
            {
                const auto existing = outputs.find(viewport.id.index);
                if (existing != outputs.end() && existing->second->cpu.id == viewport.id) {
                    if (existing->second->cpu.revision != viewport.revision) {
                        if (viewport.desc.kind == ViewportKind::NATIVE_WINDOW) {
                            const auto resized = scheduler->RequestResize(viewport.id, viewport.desc.extent);
                            if (const auto *error = std::get_if<FrameError>(&resized))
                                terminal = Error(*error);
                        }
                        existing->second      = std::make_shared<RuntimeOutput>();
                        existing->second->cpu = viewport;
                    }
                    return;
                }
                RuntimeOutput output;
                output.cpu = viewport;
                if (viewport.desc.kind == ViewportKind::NATIVE_WINDOW) {
                    SwapChain::Descriptor descriptor;
                    descriptor.window          = reinterpret_cast<void *>(viewport.desc.nativeWindow);
                    descriptor.width           = viewport.desc.extent.width;
                    descriptor.height          = viewport.desc.extent.height;
                    descriptor.preferredFormat = viewport.desc.format;
                    SwapChainPtr swapchain(device->CreateSwapChain(descriptor));
                    if (!swapchain) {
                        OutputError({viewport.id, OutputProgressKind::FAILED, {GpuErrorCode::OUT_OF_MEMORY}});
                        return;
                    }
                    auto added = scheduler->AddOutput(viewport.id, std::move(swapchain));
                    if (const auto *error = std::get_if<FrameError>(&added)) {
                        OutputError({viewport.id, OutputProgressKind::FAILED, error->cause});
                        return;
                    }
                }
                outputs[viewport.id.index] = std::make_shared<RuntimeOutput>(std::move(output));
            }
            std::optional<RendererError> CacheRaster(const RenderResourcePayload &payload)
            {
                const auto key = Key(payload.resource);
                if (payload.mesh) {
                    meshes[key] = {payload.resource, payload.mesh->streams, payload.mesh->attributes, payload.mesh->lods};
                }
                if (payload.material) {
                    const auto &source  = *payload.material;
                    auto        created = CreateFrameShader(*device, source);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return Error(*error);
                    auto                   shader = std::move(std::get<ShaderPtr>(created));
                    PreparedMaterialLayout material;
                    material.resource = payload.resource;
                    material.shader   = std::move(shader);
                    material.blend    = source.blend;
                    material.depth    = source.depth;
                    material.constants.resize(payload.bytes.size());
                    std::memcpy(material.constants.data(), payload.bytes.data(), payload.bytes.size());
                    materials[key] = std::move(material);
                }
                return {};
            }
            bool HasUploads() const
            {
                for (const auto &[key, ticket] : tickets) {
                    const auto status = catalog->GetStatus(ticket);
                    if (const auto *current = std::get_if<ResourceCatalogStatus>(&status);
                        current && current->state == RenderResourceState::CPU_READY)
                        return true;
                }
                return false;
            }
            void PublishResources()
            {
                for (auto it = tickets.begin(); it != tickets.end();) {
                    const auto  status  = catalog->GetStatus(it->second);
                    const auto *current = std::get_if<ResourceCatalogStatus>(&status);
                    if (!current) {
                        resourceStates.erase(it->first);
                        materials.erase(it->first);
                        meshes.erase(it->first);
                        it = tickets.erase(it);
                        continue;
                    }
                    const auto found = resourceStates.find(it->first);
                    if (found == resourceStates.end() || found->second != current->state) {
                        events->Push(ResourceStateChanged{it->second.resource, current->state, current->submission.serial, current->error});
                        resourceStates[it->first] = current->state;
                    }
                    if (current->state == RenderResourceState::FAILED || current->state == RenderResourceState::CANCELLED) {
                        materials.erase(it->first);
                        meshes.erase(it->first);
                    }
                    ++it;
                }
            }
            void Acknowledge(CpuSceneStore &store, const CpuRetirement &retired)
            {
                if (!store.AcknowledgeRetirement(retired))
                    return;
                std::visit(
                    [&](auto id) {
                        using T = decltype(id);
                        if constexpr (std::is_same_v<T, SceneId>)
                            events->Push(SceneDestroyed{id});
                        else if constexpr (std::is_same_v<T, RenderEntityRef>)
                            events->Push(EntityDestroyed{id});
                        else if constexpr (std::is_same_v<T, ViewId>)
                            events->Push(ViewDestroyed{id});
                        else
                            events->Push(ViewportDestroyed{id});
                    },
                    retired);
            }
            void Retire(CpuSceneStore &store)
            {
                for (auto it = retirements.begin(); it != retirements.end();) {
                    const auto older = [&](uint64_t sequence) { return sequence && sequence < it->sequence; };
                    if (std::any_of(pending.begin(), pending.end(),
                                    [&](const auto &frame) { return older(frame.inputs.prepared->packetSequence); }) ||
                        std::any_of(encoding.begin(), encoding.end(), [&](const auto &frame) { return older(frame.sequence); }) ||
                        std::any_of(submissions.begin(), submissions.end(), [&](const auto &frame) { return older(frame.second.sequence); })) {
                        ++it;
                        continue;
                    }
                    if (const auto *id = std::get_if<ViewportId>(&it->id)) {
                        const auto key = std::pair{id->index, id->generation};
                        if (closing.contains(key) && !closed.contains(key)) {
                            ++it;
                            continue;
                        }
                        closing.erase(key);
                        closed.erase(key);
                    }
                    Acknowledge(store, it->id);
                    it = retirements.erase(it);
                }
            }
            void Discard(const EncodingFrame &frame, const FrameError &error)
            {
                if (frame.sequence)
                    events->Push(RendererFailure{frame.sequence, Error(error)});
                scheduler->Abandon(frame.lease);
                if (frame.upload) {
                    const auto discarded = uploader->Discard(frame.upload);
                    if (const auto *failure = std::get_if<FrameError>(&discarded))
                        terminal = Error(*failure);
                }
                const auto discarded = context->DiscardUnsubmitted(frame.lease);
                if (const auto *failure = std::get_if<FrameError>(&discarded))
                    terminal = Error(*failure);
                if (error.code == FrameErrorCode::DEVICE_LOST || error.cause.code == GpuErrorCode::DEVICE_LOST)
                    terminal = Error(error);
            }
            void BuildFrame(FrameBuildContext frame);

            RendererInitInfo                                   init;
            Instance                                          &instance;
            std::shared_ptr<RuntimeEvents>                     events;
            std::unique_ptr<Instance::DeviceSession>           session;
            Device                                            *device = nullptr;
            std::unique_ptr<DeviceFrameContext>                context;
            std::unique_ptr<ResourceCatalog>                   catalog;
            std::unique_ptr<ResourceUploader>                  uploader;
            std::unique_ptr<FrameScheduler>                    scheduler;
            std::map<uint32_t, std::shared_ptr<RuntimeOutput>> outputs;
            std::map<uint32_t, OutputFailureDisposition>       failedOutputs;
            std::map<ResourceKey, ResourceTicket>              tickets;
            std::map<ResourceKey, RenderResourceState>         resourceStates;
            std::map<ResourceKey, PreparedMeshLayout>          meshes;
            std::map<ResourceKey, PreparedMaterialLayout>      materials;
            FrameFeatureRegistry                               features;
            std::vector<FrameExtensionPayload>                 pendingExtensions;
            std::optional<PreparedSceneProgram>                program;
            std::vector<SceneId>                               pausedScenes;
            FrozenResourceCatalog                              frozenResources;
            std::vector<OutputSnapshot>                        snapshots;
            std::vector<ViewportId>                            requestedReadbacks;
            std::deque<FrameBuildContext>                      pending;
            std::vector<EncodingFrame>                         encoding;
            std::map<uint64_t, SubmittedFrame>                 submissions;
            std::vector<RetirementRecord>                      retirements;
            std::set<std::pair<uint32_t, uint32_t>>            closed;
            std::set<std::pair<uint32_t, uint32_t>>            closing;
            std::optional<RendererError>                       terminal;
            bool                                               stopping = false;
        };

        void RendererRuntime::BuildFrame(FrameBuildContext frame)
        {
            FrameGraphBuilder                                   builder;
            OutputAcquireBatch                                  outputBatch;
            EncodingFrame                                       work;
            std::shared_ptr<const std::vector<FrameFeatureUse>> featureOwners;
            work.sequence = frame.inputs.prepared->packetSequence;
            work.lease    = frame.inputs.lease;
            // Drop graph borrowers before abandoning outputs or returning the slot.
            const auto fail = [&](const FrameError &error) {
                outputBatch.outputs.clear();
                builder.Reset();
                Discard(work, error);
            };
            std::vector<ResourceTicket> requested;
            for (const auto &[key, ticket] : tickets)
                requested.push_back(ticket);
            auto uploaded = uploader->Prepare(work.lease, builder, requested);
            if (const auto *error = std::get_if<FrameError>(&uploaded)) {
                fail(*error);
                return;
            }
            work.upload = std::get<ResourceUploadBatch>(uploaded).id;
            if (std::get<ResourceUploadBatch>(uploaded).admitted.empty()) {
                uploader->Discard(work.upload);
                work.upload = {};
            }
            std::vector<PreparedGraphBuffer>                            localBuffers;
            std::vector<std::pair<RenderResourceKey, GraphImageHandle>> localImages;
            for (auto &pin : frame.resources.pins) {
                if (pin->backing)
                    continue;
                if (auto resident = catalog->ResolveResident(pin->cpu->resource)) {
                    pin = std::move(resident);
                    continue;
                }
                const auto found = tickets.find(Key(pin->cpu->resource));
                if (found == tickets.end())
                    continue;
                const auto backing = uploader->ResolveBackingInGraph(work.upload, found->second, builder);
                if (!backing)
                    continue;
                const auto local = uploader->ResolveInGraph(work.upload, found->second, builder);
                if (const auto *handle = std::get_if<GraphBufferHandle>(&local))
                    localBuffers.push_back({pin->cpu->resource, *handle});
                if (const auto *handle = std::get_if<GraphImageHandle>(&local))
                    localImages.emplace_back(pin->cpu->resource, *handle);
                pin = std::make_shared<const ResourceVersionLease>(ResourceVersionLease{pin->cpu, backing});
            }
            std::erase_if(frame.features, [](const auto &feature) { return !feature.CanBuild(); });
            const auto enabled = [&](Name name) {
                return std::any_of(frame.features.begin(), frame.features.end(),
                                   [&](const auto &feature) { return feature.lease->registration.name == name; });
            };
            const auto needed = [&](ViewportId id) {
                return std::any_of(frame.inputs.prepared->views.begin(), frame.inputs.prepared->views.end(),
                                   [&](const auto &view) { return view.view.output == id; }) ||
                       std::any_of(frame.inputs.prepared->extensions.begin(), frame.inputs.prepared->extensions.end(), [&](const auto &payload) {
                           return enabled(payload.feature) && std::find(payload.outputs.begin(), payload.outputs.end(), id) != payload.outputs.end();
                       });
            };
            std::erase_if(frame.outputs, [&](const auto &output) { return !needed(output.viewport); });
            auto acquired = scheduler->Acquire(work.lease, frame.outputs);
            if (const auto *error = std::get_if<FrameError>(&acquired)) {
                fail(*error);
                return;
            }
            outputBatch = std::move(std::get<OutputAcquireBatch>(acquired));
            for (const auto &progress : outputBatch.skipped)
                OutputError(progress);
            FrameComposer composer(*device, *context, builder, frame.inputs);
            for (const auto &pin : frame.resources.pins) {
                const auto *backing = pin->backing ? std::get_if<ResourceImageBacking>(pin->backing.get()) : nullptr;
                if (!backing)
                    continue;
                const auto local =
                    std::find_if(localImages.begin(), localImages.end(), [&](const auto &entry) { return entry.first == pin->cpu->resource; });
                if (local != localImages.end()) {
                    composer.AddImage(local->first, local->second);
                    continue;
                }
                FrameGraphImage image;
                image.image       = backing->image;
                image.descriptor  = backing->descriptor;
                image.initial     = backing->state;
                image.final       = backing->state;
                image.initialized = true;
                if (backing->tracking) {
                    const auto tracked = context->GetResourceState(FrameResource{backing->image});
                    if (const auto *error = std::get_if<FrameError>(&tracked)) {
                        fail(*error);
                        return;
                    }
                    image.initial = std::get<ImageBoundaryState>(std::get<FrameResourceStateSnapshot>(tracked).accepted.state);
                }
                auto imported = builder.Import(std::move(image));
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    fail(*error);
                    return;
                }
                composer.AddImage(pin->cpu->resource, std::get<GraphImageHandle>(imported));
            }
            std::map<uint32_t, GraphImageHandle> renderedImages;
            for (const auto &[index, version] : frame.outputVersions) {
                auto &output = *version;
                if (!needed(output.cpu.id))
                    continue;
                FrameGraphImage  image;
                Extent2D         extent          = output.cpu.desc.extent;
                SurfaceTransform transform       = SurfaceTransform::IDENTITY;
                uint64_t         surfaceRevision = 0;
                if (output.cpu.desc.kind == ViewportKind::NATIVE_WINDOW) {
                    const auto borrowed = std::find_if(outputBatch.outputs.begin(), outputBatch.outputs.end(),
                                                       [&](const auto &item) { return item.snapshot.viewport == output.cpu.id; });
                    if (borrowed == outputBatch.outputs.end())
                        continue;
                    image           = borrowed->image;
                    extent          = borrowed->snapshot.extent;
                    transform       = borrowed->snapshot.transform;
                    surfaceRevision = borrowed->snapshot.revision;
                } else {
                    if (!extent.width || !extent.height)
                        continue;
                    if (!output.image) {
                        output.descriptor.format = output.cpu.desc.format;
                        output.descriptor.extent = {extent.width, extent.height, 1};
                        output.descriptor.usage  = ImageUsageFlagBit::RENDER_TARGET | ImageUsageFlagBit::TRANSFER_SRC | ImageUsageFlagBit::SAMPLED;
                        output.image             = ImagePtr(device->CreateImage(output.descriptor));
                        if (!output.image) {
                            fail({FrameErrorCode::OUT_OF_MEMORY, "Offscreen output allocation failed"});
                            return;
                        }
                    }
                    image.image       = output.image;
                    image.descriptor  = output.descriptor;
                    image.initial     = output.initialized ? ImageLayout::GENERAL : ImageLayout::UNDEFINED;
                    image.final       = ImageLayout::GENERAL;
                    image.initialized = output.initialized;
                }
                const auto format   = image.descriptor.format;
                const auto imported = builder.Import(std::move(image));
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    fail(*error);
                    return;
                }
                const FrameOutputInfo outputInfo{output.cpu.id, surfaceRevision, extent, format};
                frame.inputs.outputs.push_back(outputInfo);
                composer.AddOutput({outputInfo, std::get<GraphImageHandle>(imported), output.cpu.revision, transform, false},
                                   output.cpu.desc.kind == ViewportKind::OFFSCREEN);
            }
            for (auto view : frame.inputs.prepared->views) {
                const auto target = composer.GetOutput(view.view.output);
                if (!target)
                    continue;
                const auto extent = target->output.extent;
                if (target->transform != SurfaceTransform::IDENTITY)
                    view.view.projection = ViewportMapping{extent, target->transform}.ClipTransform() * view.view.projection;
                std::erase_if(view.draws, [&](const auto &draw) {
                    const auto ready = [&](const RenderResourceKey &key) {
                        return std::any_of(frame.resources.pins.begin(), frame.resources.pins.end(),
                                           [&](const auto &pin) { return pin->cpu->resource == key && pin->backing; });
                    };
                    return !ready(draw.mesh.resource) || !ready(draw.material.resource) ||
                           std::none_of(frame.meshes.begin(), frame.meshes.end(),
                                        [&](const auto &layout) { return layout.resource == draw.mesh.resource; }) ||
                           std::none_of(frame.materials.begin(), frame.materials.end(),
                                        [&](const auto &layout) { return layout.resource == draw.material.resource; });
                });
                auto resolved = ResolvePreparedRaster(*device, *context, work.lease, builder, view, frame.resources, frame.meshes, frame.materials,
                                                      {{0, 0}, extent}, localBuffers);
                if (const auto *error = std::get_if<FrameError>(&resolved)) {
                    fail(*error);
                    return;
                }
                FrameRasterPass pass;
                pass.color = target->color;
                pass.load  = target->initialized ? LoadOp::LOAD : LoadOp::CLEAR;
                pass.draws = std::move(std::get<PreparedRasterView>(resolved).draws);
                if (std::any_of(pass.draws.begin(), pass.draws.end(), [](const auto &draw) { return draw.depth.test || draw.depth.write; })) {
                    Image::Descriptor depth;
                    depth.format       = PixelFormat::D32;
                    depth.extent       = {extent.width, extent.height, 1};
                    depth.usage        = ImageUsageFlagBit::DEPTH_STENCIL;
                    const auto created = builder.CreateTransient(depth);
                    if (const auto *error = std::get_if<FrameError>(&created)) {
                        fail(*error);
                        return;
                    }
                    pass.depth = RasterDepthAttachment{std::get<GraphImageHandle>(created)};
                }
                const auto raster = composer.Raster(std::move(pass));
                if (const auto *error = std::get_if<FrameError>(&raster)) {
                    fail(*error);
                    return;
                }
                for (const auto &feature : frame.features) {
                    const auto built = feature.lease->registration.feature->BuildView(composer, view);
                    if (const auto *error = std::get_if<FrameError>(&built)) {
                        fail(*error);
                        return;
                    }
                }
            }
            for (const auto &payload : frame.inputs.prepared->extensions) {
                const auto feature = std::find_if(frame.features.begin(), frame.features.end(),
                                                  [&](const auto &entry) { return entry.lease->registration.name == payload.feature; });
                if (feature == frame.features.end())
                    continue;
                const auto built = feature->lease->registration.feature->BuildFrame(composer, payload);
                if (const auto *error = std::get_if<FrameError>(&built)) {
                    fail(*error);
                    return;
                }
            }
            const auto exported = composer.ExportOutputs();
            if (const auto *error = std::get_if<FrameError>(&exported)) {
                fail(*error);
                return;
            }
            for (const auto &[index, version] : frame.outputVersions) {
                const auto target = composer.GetOutput(version->cpu.id);
                if (target && target->initialized) {
                    renderedImages[index] = target->color;
                    if (version->cpu.desc.kind == ViewportKind::OFFSCREEN)
                        work.offscreen.push_back(version);
                } else if (version->cpu.desc.kind == ViewportKind::NATIVE_WINDOW) {
                    scheduler->AbandonOutput(work.lease, version->cpu.id);
                }
            }
            if (!frame.features.empty()) {
                featureOwners = std::make_shared<const std::vector<FrameFeatureUse>>(std::move(frame.features));
                scheduler->Retain(work.lease, featureOwners);
            }
            outputBatch.outputs.clear();
            uint64_t readbackBytes = 0;
            for (const auto id : frame.readbacks) {
                const auto found = frame.outputVersions.find(id.index);
                if (found == frame.outputVersions.end() || found->second->cpu.id != id || found->second->cpu.desc.kind != ViewportKind::OFFSCREEN ||
                    !renderedImages.contains(id.index) || found->second->cpu.desc.format != PixelFormat::RGBA8_UNORM) {
                    fail({FrameErrorCode::INVALID_ARGUMENT, "Readback requires a rendered RGBA8 offscreen viewport"});
                    return;
                }
                const auto     extent = found->second->cpu.desc.extent;
                const uint64_t pixels = uint64_t(extent.width) * extent.height;
                if (pixels > (init.maxPendingResourceBytes - readbackBytes) / 4) {
                    fail({FrameErrorCode::OUT_OF_MEMORY, "Readback byte budget exceeded"});
                    return;
                }
                readbackBytes += pixels * 4;
                Buffer::Descriptor descriptor;
                descriptor.size   = pixels * 4;
                descriptor.usage  = BufferUsageFlagBit::TRANSFER_DST;
                descriptor.memory = device->GetAPI() == API::DX12 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
                BufferPtr buffer(device->CreateBuffer(descriptor));
                if (!buffer) {
                    fail({FrameErrorCode::OUT_OF_MEMORY, "Readback allocation failed"});
                    return;
                }
                const auto imported = builder.Import(buffer, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false);
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    fail(*error);
                    return;
                }
                BufferImageCopyPass copy;
                copy.buffer             = std::get<GraphBufferHandle>(imported);
                copy.image              = renderedImages.at(id.index);
                copy.direction          = ImageCopyDirection::IMAGE_TO_BUFFER;
                copy.region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
                copy.region.imageExtent = {extent.width, extent.height, 1};
                copy.sideEffect         = true;
                const auto copied       = builder.Copy(copy);
                if (const auto *error = std::get_if<FrameError>(&copied)) {
                    fail(*error);
                    return;
                }
                work.readbacks.push_back({id, extent, PixelFormat::RGBA8_UNORM, std::move(buffer)});
            }
            const auto retained = builder.KeepAlive(std::make_shared<const std::pair<FrozenResourceCatalog, decltype(featureOwners)>>(
                std::move(frame.resources), std::move(featureOwners)));
            if (const auto *error = std::get_if<FrameError>(&retained)) {
                fail(*error);
                return;
            }
            auto frozen = builder.Freeze();
            if (const auto *error = std::get_if<FrameError>(&frozen)) {
                fail(*error);
                return;
            }
            auto compiled = context->Compile(work.lease, std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen)));
            if (std::holds_alternative<FrameNoWork>(compiled)) {
                builder.Reset();
                scheduler->Abandon(work.lease);
                if (work.upload) {
                    const auto discarded = uploader->Discard(work.upload);
                    if (const auto *error = std::get_if<FrameError>(&discarded))
                        terminal = Error(*error);
                }
                const auto discarded = context->DiscardUnsubmitted(work.lease);
                if (const auto *error = std::get_if<FrameError>(&discarded))
                    terminal = Error(*error);
                if (work.sequence) {
                    if (terminal)
                        events->Push(RendererFailure{work.sequence, *terminal});
                    else
                        events->Push(FrameCompleted{work.sequence, 0});
                }
                return;
            }
            if (const auto *error = std::get_if<FrameError>(&compiled)) {
                fail(*error);
                return;
            }
            auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto dispatched = context->Dispatch(plan);
            if (const auto *error = std::get_if<FrameError>(&dispatched)) {
                plan.reset();
                fail(*error);
                return;
            }
            work.ticket = std::get<EncodingTicket>(dispatched);
            encoding.push_back(std::move(work));
        }

        class RuntimeRenderer final : public IRenderer {
        public:
            std::optional<RendererError> Init(const RendererInitInfo &info)
            {
                owner  = std::this_thread::get_id();
                events = std::make_shared<RuntimeEvents>();
                CpuCoordinatorInit cpu;
                cpu.maxPendingPackets     = info.maxPendingPackets;
                cpu.maxOutstandingResults = info.maxPendingPackets + 1;
                cpu.prepare.workers       = info.prepareWorkers;
                return coordinator.Init(cpu, {}, std::make_unique<RendererRuntime>(info, *Instance::Get(), events));
            }
            ~RuntimeRenderer() override
            {
                coordinator.RequestShutdown();
                coordinator.Join();
            }
            PacketSubmitResult TrySubmit(std::unique_ptr<FramePacket> &packet) override
            {
                if (std::this_thread::get_id() != owner)
                    return {PacketSubmitStatus::REJECTED, {RendererErrorCode::INVALID_ARGUMENT, "Renderer submission requires its host thread"}};
                return coordinator.TrySubmit(packet);
            }
            size_t PollEvents(std::span<RendererEvent> destination) override
            {
                if (destination.empty() || std::this_thread::get_id() != owner)
                    return 0;
                bool drainCpu;
                {
                    std::lock_guard lock(events->mutex);
                    drainCpu = events->events.empty();
                }
                std::array<CpuCoordinatorEvent, 1> batch;
                if (drainCpu && coordinator.PollEvents(batch)) {
                    if (auto *packet = std::get_if<CpuPacketCompleted>(&batch[0])) {
                        if (packet->application.error)
                            events->Push(RendererFailure{packet->application.sequence, *packet->application.error});
                        else if (packet->executionError)
                            events->Push(RendererFailure{packet->application.sequence, *packet->executionError});
                        else if (!packet->preparation.Succeeded())
                            events->Push(
                                RendererFailure{packet->application.sequence, {RendererErrorCode::INTERNAL_ERROR, packet->preparation.error}});
                    } else {
                        const auto &stopped = std::get<CpuCoordinatorStopped>(batch[0]);
                        if (stopped.error) {
                            events->failed = true;
                            events->Push(RendererFailure{0, *stopped.error});
                        }
                        events->Push(RendererStopped{});
                    }
                }
                std::lock_guard lock(events->mutex);
                size_t          count = 0;
                while (count < destination.size() && !events->events.empty()) {
                    destination[count++] = std::move(events->events.front());
                    events->events.pop_front();
                }
                return count;
            }
            RendererState GetState() const override
            {
                switch (coordinator.GetState()) {
                case CpuCoordinatorState::STOPPED: return RendererState::STOPPED;
                case CpuCoordinatorState::STOPPING: return events->failed ? RendererState::FAILED : RendererState::STOPPING;
                default: return RendererState::RUNNING;
                }
            }
            RendererInfo GetInfo() const override
            {
                return events->info;
            }
            void RequestShutdown() override
            {
                coordinator.RequestShutdown();
            }

        private:
            std::thread::id                owner;
            std::shared_ptr<RuntimeEvents> events;
            CpuRenderCoordinator           coordinator;
        };
    } // namespace

    RendererCreateResult CreateRuntimeRenderer(const RendererInitInfo &info)
    {
        auto renderer = std::make_unique<RuntimeRenderer>();
        if (auto error = renderer->Init(info))
            return *error;
        return std::unique_ptr<IRenderer>(std::move(renderer));
    }
} // namespace sky::aurora
