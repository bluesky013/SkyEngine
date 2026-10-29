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
        // GPU work owns snapshots collected after ECS preparation and borrower drain.
        struct PreparedRenderFrame {
            FrameBuildInputs                                   inputs;
            FrozenResourceCatalog                              resources;
            std::vector<OutputSnapshot>                        outputs;
            std::vector<ViewportId>                            readbacks;
            std::map<uint32_t, std::shared_ptr<RuntimeOutput>> outputVersions;
            std::vector<PreparedMeshLayout>                    meshes;
            std::vector<PreparedMaterialTechniques>            materials;
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
        struct FrameGraphWork {
            FrameGraphBuilder                                           builder;
            OutputAcquireBatch                                          outputBatch;
            EncodingFrame                                               encoding;
            std::vector<PreparedGraphBuffer>                            localBuffers;
            std::vector<std::pair<RenderResourceKey, GraphImageHandle>> localImages;
            std::map<uint32_t, GraphImageHandle>                        renderedImages;
            std::shared_ptr<const std::vector<FrameFeatureUse>>         featureOwners;
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
            std::optional<RendererError> Initialize() override;
            std::optional<RendererError> Validate(const FramePacket &packet) const override;
            std::optional<RendererError> Applied(CpuSceneStore &store, FramePacket &packet, const PacketApplyResult &applied) override
            {
                ApplyPacketEffects(store, packet, applied);
                SynchronizeSceneResources(store, packet.sequence, applied.changedScenes);
                SupplyCpuResources(packet);
                FreezePrepareResources(store);
                for (const auto &key : packet.releasedResources)
                    catalog->Release(key);
                RefreshResources(store);
                SnapshotOutputs(store);
                return terminal;
            }
            // Declares ECS work; CpuRenderCoordinator submits it to ScenePrepareScheduler.
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
                auto built = BuildPreparedSceneProgram(frame, prepareResources.cpu, 1024, pausedScenes);
                if (const auto *error = std::get_if<RendererError>(&built))
                    return *error;
                scenePreparation                     = std::move(std::get<PreparedSceneProgram>(built));
                scenePreparation->frame.extensions   = std::move(pendingExtensions);
                scenePreparation->frame.timeSeconds  = pendingTime;
                scenePreparation->frame.deltaSeconds = pendingDelta;
                return std::move(scenePreparation->work);
            }
            bool                         RecoverPrepareFailure(const CpuPacketCompleted &result) override;
            std::optional<RendererError> Prepared(CpuSceneStore &store, const CpuPacketCompleted &result) override
            {
                if (!scenePreparation || !result.preparation.Succeeded()) {
                    scenePreparation.reset();
                    prepareResources = {};
                    return {};
                }
                auto collected = CollectPreparedScene(*scenePreparation, result.preparation);
                scenePreparation.reset();
                if (const auto *error = std::get_if<RendererError>(&collected))
                    return *error;
                while (pending.size() >= init.maxPendingPackets) {
                    if (auto error = Pump(store)) {
                        terminal = std::move(error);
                        events->Push(RendererFailure{result.application.sequence, *terminal});
                        prepareResources = {};
                        return {};
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                QueuePreparedFrame(std::move(std::get<std::shared_ptr<const PreparedFrame>>(collected)));
                // GPU work owns the frame's final result; the CPU acknowledgement must not duplicate it.
                if (auto error = Pump(store))
                    terminal = std::move(error);
                return {};
            }
            std::optional<RendererError> Pump(CpuSceneStore &store) override
            {
                if (!uploader || !scheduler)
                    return {};
                PollSubmissions();
                PumpOutputs();
                SubmitEncodedFrames();
                RefreshResources(store);
                if (terminal)
                    return terminal;
                if (auto error = StartPendingFrame())
                    return error;
                Retire(store);
                for (const auto id : features.CollectRetired())
                    events->Push(RenderFeatureRetired{id});
                return terminal;
            }
            std::optional<RendererError> Shutdown(CpuSceneStore &store) override;

        private:
            void                         OutputError(const OutputProgress &progress);
            void                         UpdateOutput(const CpuViewportState &viewport);
            std::optional<RendererError> CacheRaster(const RenderResourcePayload &payload);
            bool                         HasUploads() const;
            void                         PublishResources();
            void                         Acknowledge(CpuSceneStore &store, const CpuRetirement &retired);
            void                         Retire(CpuSceneStore &store);
            void                         Discard(const EncodingFrame &frame, const FrameError &error);
            void                         ApplyPacketEffects(CpuSceneStore &store, FramePacket &packet, const PacketApplyResult &applied);
            void                         SynchronizeSceneResources(CpuSceneStore &store, uint64_t sequence, std::span<const SceneId> changedScenes);
            void                         SupplyCpuResources(FramePacket &packet);
            void                         FreezePrepareResources(CpuSceneStore &store);
            void                         SnapshotOutputs(CpuSceneStore &store);
            void                         QueuePreparedFrame(std::shared_ptr<const PreparedFrame> prepared);
            void                         PollSubmissions();
            void                         PumpOutputs();
            void                         SubmitEncodedFrames();
            void                         RefreshResources(CpuSceneStore &store);
            std::optional<RendererError> StartPendingFrame();
            std::optional<FrameError>    PrepareFrameFeatures(PreparedRenderFrame &frame);
            std::optional<FrameError>    PrepareGraphResources(PreparedRenderFrame &frame, FrameGraphWork &graph);
            bool                         NeedsOutput(const PreparedRenderFrame &frame, ViewportId id) const;
            std::optional<FrameError>    AcquireFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph);
            std::optional<FrameError>    ImportResourceImages(const PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer);
            std::optional<FrameError>    ImportFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer);
            std::optional<FrameError>    BuildFeaturePasses(const PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer);
            std::optional<FrameError>    ExportFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer);
            std::optional<FrameError>    AppendReadbacks(const PreparedRenderFrame &frame, FrameGraphWork &graph);
            std::optional<FrameError>    DispatchFrame(PreparedRenderFrame &frame, FrameGraphWork &graph);
            void                         BuildFrame(PreparedRenderFrame frame);

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
            std::map<ResourceKey, PreparedMaterialTechniques>  materials;
            FrameFeatureRegistry                               features;
            std::vector<FrameExtensionPayload>                 pendingExtensions;
            double                                             pendingTime  = 0;
            float                                              pendingDelta = 0;
            std::optional<PreparedSceneProgram>                scenePreparation;
            std::vector<SceneId>                               pausedScenes;
            FrozenResourceCatalog                              prepareResources;
            std::vector<RenderResourceKey>                     externalDependencies;
            std::vector<OutputSnapshot>                        snapshots;
            std::vector<ViewportId>                            requestedReadbacks;
            std::deque<PreparedRenderFrame>                    pending;
            std::vector<EncodingFrame>                         encoding;
            std::map<uint64_t, SubmittedFrame>                 submissions;
            std::vector<RetirementRecord>                      retirements;
            std::set<std::pair<uint32_t, uint32_t>>            closed;
            std::set<std::pair<uint32_t, uint32_t>>            closing;
            std::optional<RendererError>                       terminal;
            bool                                               stopping = false;
        };

        std::optional<RendererError> RendererRuntime::Initialize()
        {
            auto acquired = instance.Acquire({"Aurora Renderer", "SkyEngine", init.enableValidation, init.backend, init.vulkan});
            if (const auto *error = std::get_if<GpuError>(&acquired))
                return RendererError{error->code == GpuErrorCode::INVALID_ARGUMENT ? RendererErrorCode::NOT_READY : RendererErrorCode::UNSUPPORTED,
                                     "Renderer requires an available, exclusive backend device"};
            session = std::move(std::get<std::unique_ptr<Instance::DeviceSession>>(acquired));
            device  = session->GetDevice();
            DeviceFrameContextInitInfo info;
            info.inflightNum              = init.inflightFrames;
            info.parallelNum              = init.encodeWorkers;
            info.deviceEpoch              = session->GetEpoch();
            info.allocations.stagingBytes = init.maxUploadBytesPerFrame;
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

        std::optional<RendererError> RendererRuntime::Validate(const FramePacket &packet) const
        {
            if (auto error = features.Validate(packet))
                return error;
            uint64_t   bytes    = 0;
            const auto validKey = [&](const RenderResourceKey &key) {
                return key.product && key.revision && key.deviceEpoch == events->info.deviceEpoch;
            };
            if (packet.releasedResources.size() > 4096 || !std::all_of(packet.releasedResources.begin(), packet.releasedResources.end(), validKey))
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
                    payload.format.Empty() || (!payload.material && payload.bytes.empty()) || payload.bytes.size() % 4 ||
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
                if (payload.material) {
                    if (payload.material->techniques.empty())
                        return RendererError{RendererErrorCode::INVALID_PACKET, "Material requires explicit techniques"};
                    std::set<Name> tags;
                    for (const auto &technique : payload.material->techniques) {
                        if (technique.tag.Empty() || !tags.insert(technique.tag).second)
                            return RendererError{RendererErrorCode::INVALID_PACKET, "Material requires unique nonempty Tags"};
                        const auto validate = [&](const RenderTechniqueData &program) -> std::optional<RendererError> {
                            if (program.compute || program.backend != device->GetAPI() || !program.vertex || !program.fragment)
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Material requires graphics code for the backend"};
                            if (const auto error = ValidateFrameShader(program))
                                return RendererError{RendererErrorCode::INVALID_PACKET, error->message};
                            std::string error;
                            if (!ValidateRenderMaterialParameters(program, technique.parameters, payload.resource.deviceEpoch, error))
                                return RendererError{RendererErrorCode::INVALID_PACKET, std::move(error)};
                            return {};
                        };
                        if (const auto error = validate(technique.program))
                            return error;
                        std::set<std::tuple<bool, uint16_t, std::map<Name, uint32_t>>> selections;
                        for (const auto &variant : technique.variants) {
                            if (RenderMaterialInterfaceFingerprint(variant.program, 2) != RenderMaterialInterfaceFingerprint(technique.program, 2))
                                return RendererError{RendererErrorCode::INVALID_PACKET,
                                                     "Material declaration changes across Pipeline or Vertex variants"};
                            std::map<Name, uint32_t> pipeline;
                            for (const auto &entry : variant.pipeline.entries)
                                if (entry.key.Empty() || !pipeline.emplace(entry.key, entry.value).second)
                                    return RendererError{RendererErrorCode::INVALID_PACKET, "Material variant has duplicate or empty Pipeline keys"};
                            if (!selections.emplace(variant.skinned, variant.vertexMask, std::move(pipeline)).second)
                                return RendererError{RendererErrorCode::INVALID_PACKET, "Material has duplicate variant selections"};
                            if (const auto error = validate(variant.program))
                                return error;
                        }
                    }
                }
            }
            return {};
        }

        void RendererRuntime::ApplyPacketEffects(CpuSceneStore &store, FramePacket &packet, const PacketApplyResult &applied)
        {
            features.Apply(packet);
            pendingExtensions = std::move(packet.extensions);
            pendingTime       = packet.timeSeconds;
            pendingDelta      = packet.deltaSeconds;
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
        }

        void RendererRuntime::SynchronizeSceneResources(CpuSceneStore &store, uint64_t sequence, std::span<const SceneId> changedScenes)
        {
            auto resourceScenes = store.SelectPrepareInputs().scenes;
            // Binding updates must survive a suspended output, before catalog refresh/pruning sees stale subscribers.
            for (const auto id : changedScenes)
                if (std::find(resourceScenes.begin(), resourceScenes.end(), id) == resourceScenes.end())
                    resourceScenes.push_back(id);
            for (const auto id : resourceScenes) {
                const auto *scene = store.FindScene(id);
                if (!scene)
                    continue;
                for (const auto &[index, entity] : scene->entities) {
                    const auto *mesh     = scene->registry.Get<RenderMeshBinding>(entity.entity);
                    const auto *material = scene->registry.Get<RenderMaterialBinding>(entity.entity);
                    for (const auto slot : {ResourceBindingSlot::MESH, ResourceBindingSlot::MATERIAL}) {
                        const auto &binding = slot == ResourceBindingSlot::MESH ? mesh->binding : material->binding;
                        if (!binding.resource.product)
                            continue;
                        const auto requested = catalog->Request(store, {{id, entity.id}, slot, binding.bindingRevision, sequence});
                        if (const auto *request = std::get_if<ResourceRequest>(&requested))
                            tickets[Key(request->ticket.resource)] = request->ticket;
                        else
                            events->Push(ResourceStateChanged{binding.resource, RenderResourceState::FAILED, 0, std::get<RendererError>(requested)});
                    }
                }
            }
        }

        void RendererRuntime::SupplyCpuResources(FramePacket &packet)
        {
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
                const bool material = payload.material.has_value();
                if (auto error = CacheRaster(payload)) {
                    catalog->Fail(request.ticket, *error);
                } else if (auto error = catalog->Supply(request.ticket, payload)) {
                    catalog->Fail(request.ticket, *error);
                } else if (material) {
                    if (auto error = catalog->AdoptResident(request.ticket, key, std::make_shared<const ResourceBacking>(ResourceBufferBacking{})))
                        catalog->Fail(request.ticket, *error);
                }
            }
        }

        void RendererRuntime::FreezePrepareResources(CpuSceneStore &store)
        {
            std::erase_if(pausedScenes, [&](SceneId id) { return !store.FindScene(id); });
            std::vector<RenderResourceKey> external;
            for (const auto &payload : pendingExtensions)
                external.insert(external.end(), payload.resources.begin(), payload.resources.end());
            prepareResources         = catalog->Freeze(store, true, external);
            const auto originalCount = external.size();
            for (const auto &pin : prepareResources.pins) {
                const auto material = materials.find(Key(pin->cpu->resource));
                if (material == materials.end())
                    continue;
                for (const auto &[tag, technique] : material->second.techniques)
                    for (const auto &image : technique.parameters.images) {
                        ShaderParameterHandle handle;
                        if (technique.shader->GetParameterTable().Resolve(image.name, 2, handle) == ShaderParameterLookup::READY &&
                            std::find(external.begin(), external.end(), image.resource) == external.end())
                            external.push_back(image.resource);
                    }
            }
            if (external.size() != originalCount)
                prepareResources = catalog->Freeze(store, true, external);
            externalDependencies = std::move(external);
        }

        void RendererRuntime::SnapshotOutputs(CpuSceneStore &store)
        {
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
        }

        bool RendererRuntime::RecoverPrepareFailure(const CpuPacketCompleted &result)
        {
            if (!scenePreparation || result.preparation.cancelled)
                return false;
            bool isolated = false;
            for (const auto &node : result.preparation.nodes) {
                if (node.state != PrepareNodeState::FAILED)
                    continue;
                SceneId id;
                for (const auto &[system, scene] : scenePreparation->sceneSystems)
                    if (system == node.system)
                        id = scene;
                for (size_t i = 0; i < scenePreparation->viewSystems.size(); ++i)
                    if (scenePreparation->viewSystems[i] == node.system)
                        id = scenePreparation->frame.views[i].view.scene;
                if (!id.IsValid())
                    return false;
                if (std::find(pausedScenes.begin(), pausedScenes.end(), id) == pausedScenes.end())
                    pausedScenes.push_back(id);
                events->Push(ScenePrepareFailed{id, result.application.sequence, {RendererErrorCode::INTERNAL_ERROR, node.diagnostic}});
                isolated = true;
            }
            return isolated;
        }

        void RendererRuntime::QueuePreparedFrame(std::shared_ptr<const PreparedFrame> prepared)
        {
            PreparedRenderFrame frame;
            frame.inputs.prepared = std::move(prepared);
            frame.resources       = std::move(prepareResources);
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
        }

        void RendererRuntime::BuildFrame(PreparedRenderFrame frame)
        {
            FrameGraphWork graph;
            graph.encoding.sequence = frame.inputs.prepared->packetSequence;
            graph.encoding.lease    = frame.inputs.lease;
            // Drop graph borrowers before abandoning outputs or returning the slot.
            const auto fail = [&](const FrameError &error) {
                graph.outputBatch.outputs.clear();
                graph.builder.Reset();
                Discard(graph.encoding, error);
            };
            if (auto error = PrepareFrameFeatures(frame)) {
                fail(*error);
                return;
            }
            if (auto error = PrepareGraphResources(frame, graph)) {
                fail(*error);
                return;
            }
            if (auto error = AcquireFrameOutputs(frame, graph)) {
                fail(*error);
                return;
            }
            FrameComposer composer(*device, *context, graph.builder, frame.inputs);
            if (auto error = ImportResourceImages(frame, graph, composer)) {
                fail(*error);
                return;
            }
            if (auto error = ImportFrameOutputs(frame, graph, composer)) {
                fail(*error);
                return;
            }
            if (auto error = BuildFeaturePasses(frame, graph, composer)) {
                fail(*error);
                return;
            }
            if (auto error = ExportFrameOutputs(frame, graph, composer)) {
                fail(*error);
                return;
            }
            if (auto error = AppendReadbacks(frame, graph)) {
                fail(*error);
                return;
            }
            if (auto error = DispatchFrame(frame, graph)) {
                fail(*error);
                return;
            }
        }

        std::optional<FrameError> RendererRuntime::PrepareFrameFeatures(PreparedRenderFrame &frame)
        {
            class FeaturePreparation final : public FrameFeaturePrepareContext {
            public:
                FeaturePreparation(Device &device, RendererInfo info, const FrameBuildInputs &inputs) : device(device), info(info), inputs(inputs)
                {
                }
                RendererInfo GetRendererInfo() const override
                {
                    return info;
                }
                const FrameBuildInputs &GetInputs() const override
                {
                    return inputs;
                }
                std::variant<FrameError, ShaderPtr> CreateShader(const RenderTechniqueData &source) override
                {
                    return CreateFrameShader(device, source);
                }

            private:
                Device                 &device;
                RendererInfo            info;
                const FrameBuildInputs &inputs;
            } preparation(*device, events->info, frame.inputs);
            std::erase_if(frame.features, [](const auto &feature) { return !feature.CanBuild(); });
            for (const auto &feature : frame.features) {
                const auto prepared = feature.lease->registration.feature->Prepare(preparation);
                if (const auto *error = std::get_if<FrameError>(&prepared)) {
                    return *error;
                }
            }
            return {};
        }

        std::optional<FrameError> RendererRuntime::PrepareGraphResources(PreparedRenderFrame &frame, FrameGraphWork &graph)
        {
            std::vector<ResourceTicket> requested;
            for (const auto &[key, ticket] : tickets)
                requested.push_back(ticket);
            auto uploaded = uploader->Prepare(graph.encoding.lease, graph.builder, requested);
            if (const auto *error = std::get_if<FrameError>(&uploaded)) {
                return *error;
            }
            graph.encoding.upload = std::get<ResourceUploadBatch>(uploaded).id;
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
                const auto backing = uploader->ResolveBackingInGraph(graph.encoding.upload, found->second, graph.builder);
                if (!backing)
                    continue;
                const auto local = uploader->ResolveInGraph(graph.encoding.upload, found->second, graph.builder);
                if (const auto *handle = std::get_if<GraphBufferHandle>(&local))
                    graph.localBuffers.push_back({pin->cpu->resource, *handle});
                if (const auto *handle = std::get_if<GraphImageHandle>(&local))
                    graph.localImages.emplace_back(pin->cpu->resource, *handle);
                pin = std::make_shared<const ResourceVersionLease>(ResourceVersionLease{pin->cpu, backing});
            }
            return {};
        }

        bool RendererRuntime::NeedsOutput(const PreparedRenderFrame &frame, ViewportId id) const
        {
            const auto enabled = [&](Name name) {
                return std::any_of(frame.features.begin(), frame.features.end(),
                                   [&](const auto &feature) { return feature.lease->registration.name == name; });
            };
            return std::any_of(frame.inputs.prepared->views.begin(), frame.inputs.prepared->views.end(),
                               [&](const auto &view) { return view.view.output == id; }) ||
                   std::any_of(frame.inputs.prepared->extensions.begin(), frame.inputs.prepared->extensions.end(), [&](const auto &payload) {
                       return enabled(payload.feature) && std::find(payload.outputs.begin(), payload.outputs.end(), id) != payload.outputs.end();
                   });
        }

        std::optional<FrameError> RendererRuntime::AcquireFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph)
        {
            std::erase_if(frame.features, [](const auto &feature) { return !feature.CanBuild(); });
            std::erase_if(frame.outputs, [&](const auto &output) { return !NeedsOutput(frame, output.viewport); });
            auto acquired = scheduler->Acquire(graph.encoding.lease, frame.outputs);
            if (const auto *error = std::get_if<FrameError>(&acquired)) {
                return *error;
            }
            graph.outputBatch = std::move(std::get<OutputAcquireBatch>(acquired));
            for (const auto &progress : graph.outputBatch.skipped)
                OutputError(progress);
            return {};
        }

        std::optional<FrameError>
        RendererRuntime::ImportResourceImages(const PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer)
        {
            for (const auto &pin : frame.resources.pins) {
                const auto *backing = pin->backing ? std::get_if<ResourceImageBacking>(pin->backing.get()) : nullptr;
                if (!backing)
                    continue;
                const auto local = std::find_if(graph.localImages.begin(), graph.localImages.end(),
                                                [&](const auto &entry) { return entry.first == pin->cpu->resource; });
                if (local != graph.localImages.end()) {
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
                        return *error;
                    }
                    image.initial = std::get<ImageBoundaryState>(std::get<FrameResourceStateSnapshot>(tracked).accepted.state);
                }
                auto imported = graph.builder.Import(std::move(image));
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    return *error;
                }
                composer.AddImage(pin->cpu->resource, std::get<GraphImageHandle>(imported));
            }
            return {};
        }

        std::optional<FrameError> RendererRuntime::ImportFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer)
        {
            for (const auto &[index, version] : frame.outputVersions) {
                auto &output = *version;
                if (!NeedsOutput(frame, output.cpu.id))
                    continue;
                FrameGraphImage  image;
                Extent2D         extent          = output.cpu.desc.extent;
                SurfaceTransform transform       = SurfaceTransform::IDENTITY;
                uint64_t         surfaceRevision = 0;
                if (output.cpu.desc.kind == ViewportKind::NATIVE_WINDOW) {
                    const auto borrowed = std::find_if(graph.outputBatch.outputs.begin(), graph.outputBatch.outputs.end(),
                                                       [&](const auto &item) { return item.snapshot.viewport == output.cpu.id; });
                    if (borrowed == graph.outputBatch.outputs.end())
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
                            return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Offscreen output allocation failed"};
                        }
                    }
                    image.image       = output.image;
                    image.descriptor  = output.descriptor;
                    image.initial     = output.initialized ? ImageLayout::GENERAL : ImageLayout::UNDEFINED;
                    image.final       = ImageLayout::GENERAL;
                    image.initialized = output.initialized;
                }
                const auto format   = image.descriptor.format;
                const auto imported = graph.builder.Import(std::move(image));
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    return *error;
                }
                const FrameOutputInfo outputInfo{output.cpu.id, surfaceRevision, extent, format};
                frame.inputs.outputs.push_back(outputInfo);
                composer.AddOutput({outputInfo, std::get<GraphImageHandle>(imported), output.cpu.revision, transform, false},
                                   output.cpu.desc.kind == ViewportKind::OFFSCREEN);
            }
            return {};
        }

        std::optional<FrameError>
        RendererRuntime::BuildFeaturePasses(const PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer)
        {
            for (auto view : frame.inputs.prepared->views) {
                const auto target = composer.GetOutput(view.view.output);
                if (!target)
                    continue;
                const auto unavailable = [&](const auto &draw) {
                    const auto ready = [&](const RenderResourceKey &key) {
                        return std::any_of(frame.resources.pins.begin(), frame.resources.pins.end(),
                                           [&](const auto &pin) { return pin->cpu->resource == key && pin->backing; });
                    };
                    const auto material = std::find_if(frame.materials.begin(), frame.materials.end(),
                                                       [&](const auto &layout) { return layout.resource == draw.material.resource; });
                    if (material == frame.materials.end())
                        return true;
                    for (const auto &[tag, technique] : material->techniques)
                        for (const auto &image : technique.parameters.images) {
                            ShaderParameterHandle handle;
                            if (technique.shader->GetParameterTable().Resolve(image.name, 2, handle) == ShaderParameterLookup::READY &&
                                !ready(image.resource))
                                return true;
                        }
                    return !ready(draw.mesh.resource) || !ready(draw.material.resource) ||
                           std::none_of(frame.meshes.begin(), frame.meshes.end(),
                                        [&](const auto &layout) { return layout.resource == draw.mesh.resource; });
                };
                std::erase_if(view.draws, unavailable);
                std::erase_if(view.shadowCasters, unavailable);
                composer.SetPreparedView(&view, frame.resources, frame.meshes, frame.materials, graph.localBuffers);
                for (const auto &feature : frame.features) {
                    const auto built = feature.lease->registration.feature->BuildView(composer, view);
                    if (const auto *error = std::get_if<FrameError>(&built)) {
                        composer.SetPreparedView(nullptr, frame.resources, {}, {}, {});
                        return *error;
                    }
                }
            }
            composer.SetPreparedView(nullptr, frame.resources, {}, {}, {});
            for (const auto &payload : frame.inputs.prepared->extensions) {
                const auto feature = std::find_if(frame.features.begin(), frame.features.end(),
                                                  [&](const auto &entry) { return entry.lease->registration.name == payload.feature; });
                if (feature == frame.features.end())
                    continue;
                const auto built = feature->lease->registration.feature->BuildFrame(composer, payload);
                if (const auto *error = std::get_if<FrameError>(&built)) {
                    return *error;
                }
            }
            return {};
        }

        std::optional<FrameError> RendererRuntime::ExportFrameOutputs(PreparedRenderFrame &frame, FrameGraphWork &graph, FrameComposer &composer)
        {
            const auto exported = composer.ExportOutputs();
            if (const auto *error = std::get_if<FrameError>(&exported)) {
                return *error;
            }
            for (const auto &[index, version] : frame.outputVersions) {
                const auto target = composer.GetOutput(version->cpu.id);
                if (target && target->initialized) {
                    graph.renderedImages[index] = target->color;
                    if (version->cpu.desc.kind == ViewportKind::OFFSCREEN)
                        graph.encoding.offscreen.push_back(version);
                } else if (version->cpu.desc.kind == ViewportKind::NATIVE_WINDOW) {
                    scheduler->AbandonOutput(graph.encoding.lease, version->cpu.id);
                }
            }
            if (!frame.features.empty()) {
                graph.featureOwners = std::make_shared<const std::vector<FrameFeatureUse>>(std::move(frame.features));
                scheduler->Retain(graph.encoding.lease, graph.featureOwners);
            }
            graph.outputBatch.outputs.clear();
            return {};
        }

        std::optional<FrameError> RendererRuntime::AppendReadbacks(const PreparedRenderFrame &frame, FrameGraphWork &graph)
        {
            uint64_t readbackBytes = 0;
            for (const auto id : frame.readbacks) {
                const auto found = frame.outputVersions.find(id.index);
                if (found == frame.outputVersions.end() || found->second->cpu.id != id || found->second->cpu.desc.kind != ViewportKind::OFFSCREEN ||
                    !graph.renderedImages.contains(id.index) || found->second->cpu.desc.format != PixelFormat::RGBA8_UNORM) {
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Readback requires a rendered RGBA8 offscreen viewport"};
                }
                const auto     extent = found->second->cpu.desc.extent;
                const uint64_t pixels = uint64_t(extent.width) * extent.height;
                if (pixels > (init.maxPendingResourceBytes - readbackBytes) / 4) {
                    return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Readback byte budget exceeded"};
                }
                readbackBytes += pixels * 4;
                Buffer::Descriptor descriptor;
                descriptor.size   = pixels * 4;
                descriptor.usage  = BufferUsageFlagBit::TRANSFER_DST;
                descriptor.memory = device->GetAPI() == API::DX12 ? MemoryType::GPU_TO_CPU : MemoryType::CPU_ONLY;
                BufferPtr buffer(device->CreateBuffer(descriptor));
                if (!buffer) {
                    return FrameError{FrameErrorCode::OUT_OF_MEMORY, "Readback allocation failed"};
                }
                const auto imported = graph.builder.Import(buffer, CopyBufferState::HOST_READ, CopyBufferState::HOST_READ, false);
                if (const auto *error = std::get_if<FrameError>(&imported)) {
                    return *error;
                }
                BufferImageCopyPass copy;
                copy.buffer             = std::get<GraphBufferHandle>(imported);
                copy.image              = graph.renderedImages.at(id.index);
                copy.direction          = ImageCopyDirection::IMAGE_TO_BUFFER;
                copy.region.subRange    = {0, 0, 1, AspectFlagBit::COLOR_BIT};
                copy.region.imageExtent = {extent.width, extent.height, 1};
                copy.sideEffect         = true;
                const auto copied       = graph.builder.Copy(copy);
                if (const auto *error = std::get_if<FrameError>(&copied)) {
                    return *error;
                }
                graph.encoding.readbacks.push_back({id, extent, PixelFormat::RGBA8_UNORM, std::move(buffer)});
            }
            return {};
        }

        std::optional<FrameError> RendererRuntime::DispatchFrame(PreparedRenderFrame &frame, FrameGraphWork &graph)
        {
            const auto retained = graph.builder.KeepAlive(std::make_shared<const std::pair<FrozenResourceCatalog, decltype(graph.featureOwners)>>(
                std::move(frame.resources), std::move(graph.featureOwners)));
            if (const auto *error = std::get_if<FrameError>(&retained)) {
                return *error;
            }
            auto frozen = graph.builder.Freeze();
            if (const auto *error = std::get_if<FrameError>(&frozen)) {
                return *error;
            }
            auto compiled = context->Compile(graph.encoding.lease, std::move(std::get<std::shared_ptr<const FrozenFrameGraph>>(frozen)));
            if (std::holds_alternative<FrameNoWork>(compiled)) {
                graph.builder.Reset();
                scheduler->Abandon(graph.encoding.lease);
                if (graph.encoding.upload) {
                    const auto discarded = uploader->Discard(graph.encoding.upload);
                    if (const auto *error = std::get_if<FrameError>(&discarded))
                        terminal = Error(*error);
                }
                const auto discarded = context->DiscardUnsubmitted(graph.encoding.lease);
                if (const auto *error = std::get_if<FrameError>(&discarded))
                    terminal = Error(*error);
                if (graph.encoding.sequence) {
                    if (terminal)
                        events->Push(RendererFailure{graph.encoding.sequence, *terminal});
                    else {
                        events->Push(FrameCompleted{graph.encoding.sequence, 0});
                    }
                }
                return {};
            }
            if (const auto *error = std::get_if<FrameError>(&compiled)) {
                return *error;
            }
            auto plan       = std::move(std::get<std::unique_ptr<BackendCompiledFrame>>(compiled));
            auto dispatched = context->Dispatch(plan);
            if (const auto *error = std::get_if<FrameError>(&dispatched)) {
                plan.reset();
                return *error;
            }
            graph.encoding.ticket = std::get<EncodingTicket>(dispatched);
            encoding.push_back(std::move(graph.encoding));
            return {};
        }

        void RendererRuntime::PollSubmissions()
        {
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
                    else {
                        events->Push(FrameCompleted{found->second.sequence, completion.token.serial});
                    }
                }
                if (found != submissions.end())
                    submissions.erase(found);
            }
        }

        void RendererRuntime::PumpOutputs()
        {
            std::array<OutputProgress, 64> progress;
            const auto                     count = scheduler->Pump(progress);
            for (const auto &output : std::span(progress).first(count)) {
                if (output.kind == OutputProgressKind::CLOSED)
                    closed.insert({output.viewport.index, output.viewport.generation});
                OutputError(output);
            }
        }

        void RendererRuntime::SubmitEncodedFrames()
        {
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
        }

        void RendererRuntime::RefreshResources(CpuSceneStore &store)
        {
            PublishResources();
            if (catalog->Prune(store, externalDependencies))
                PublishResources();
        }

        std::optional<RendererError> RendererRuntime::StartPendingFrame()
        {
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
                    PreparedRenderFrame frame;
                    frame.inputs.prepared = std::make_shared<const PreparedFrame>();
                    pending.push_back(std::move(frame));
                }
            }
            return {};
        }

        std::optional<RendererError> RendererRuntime::Shutdown(CpuSceneStore &store)
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
            if (device)
                device->WaitIdle();
            pending.clear();
            encoding.clear();
            submissions.clear();
            prepareResources = {};
            scenePreparation.reset();
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
                        catalog->Fail(ticket,
                                      terminal.value_or(RendererError{RendererErrorCode::NOT_READY, "Renderer stopped before resource completion"}));
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

        void RendererRuntime::OutputError(const OutputProgress &progress)
        {
            if (progress.kind != OutputProgressKind::FAILED)
                return;
            const auto    code    = progress.error.code == GpuErrorCode::DEVICE_LOST   ? RendererErrorCode::DEVICE_LOST
                                    : progress.error.code == GpuErrorCode::UNSUPPORTED ? RendererErrorCode::UNSUPPORTED
                                                                                       : RendererErrorCode::NOT_READY;
            const char   *message = code == RendererErrorCode::UNSUPPORTED ? "Viewport presentation is unsupported" : "Viewport is unavailable";
            RendererError error{code, std::string(message) + " (native=" + std::to_string(progress.error.nativeCode) + ")"};
            const auto    reported = failedOutputs.find(progress.viewport.index);
            if (reported == failedOutputs.end() || reported->second != progress.disposition) {
                events->Push(ViewportFailed{progress.viewport, error, progress.disposition == OutputFailureDisposition::REPLACE});
                failedOutputs[progress.viewport.index] = progress.disposition;
            }
            if (error.code == RendererErrorCode::DEVICE_LOST || error.code == RendererErrorCode::UNSUPPORTED ||
                progress.disposition == OutputFailureDisposition::SHUTDOWN)
                terminal = error;
        }

        void RendererRuntime::UpdateOutput(const CpuViewportState &viewport)
        {
            if (terminal)
                return;
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

        std::optional<RendererError> RendererRuntime::CacheRaster(const RenderResourcePayload &payload)
        {
            const auto key = Key(payload.resource);
            if (payload.mesh) {
                meshes[key] = {payload.resource, payload.mesh->streams, payload.mesh->attributes, payload.mesh->lods};
            }
            if (payload.material) {
                PreparedMaterialTechniques material;
                material.resource = payload.resource;
                const auto add    = [&](const RenderTechniqueData &source, const ShaderVariant &pipeline, bool skinned, bool pipelineAgnostic,
                                     uint16_t vertexMask, Name tag, const RenderMaterialParameters &parameters) -> std::optional<RendererError> {
                    auto created = CreateFrameShader(*device, source);
                    if (const auto *error = std::get_if<FrameError>(&created))
                        return Error(*error);
                    PreparedMaterialLayout layout;
                    layout.resource         = payload.resource;
                    layout.shader           = std::move(std::get<ShaderPtr>(created));
                    layout.pipeline         = pipeline;
                    layout.skinned          = skinned;
                    layout.pipelineAgnostic = pipelineAgnostic;
                    layout.vertexMask       = vertexMask;
                    layout.blend            = source.blend;
                    layout.depth            = source.depth;
                    layout.parameters       = parameters;
                    material.techniques.emplace_back(tag, std::move(layout));
                    return {};
                };
                for (const auto &technique : payload.material->techniques) {
                    if (!technique.variants.empty()) {
                        for (const auto &variant : technique.variants)
                            if (const auto error = add(variant.program, variant.pipeline, variant.skinned, false, variant.vertexMask, technique.tag,
                                                       technique.parameters))
                                return error;
                    } else if (const auto error = add(technique.program, {}, false, true, 0, technique.tag, technique.parameters)) {
                        return error;
                    }
                }
                materials[key] = std::move(material);
            }
            return {};
        }

        bool RendererRuntime::HasUploads() const
        {
            for (const auto &[key, ticket] : tickets) {
                const auto status = catalog->GetStatus(ticket);
                if (const auto *current = std::get_if<ResourceCatalogStatus>(&status); current && current->state == RenderResourceState::CPU_READY)
                    return true;
            }
            return false;
        }

        void RendererRuntime::PublishResources()
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

        void RendererRuntime::Acknowledge(CpuSceneStore &store, const CpuRetirement &retired)
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

        void RendererRuntime::Retire(CpuSceneStore &store)
        {
            for (auto it = retirements.begin(); it != retirements.end();) {
                const auto older = [&](uint64_t sequence) { return sequence && sequence < it->sequence; };
                if (std::any_of(pending.begin(), pending.end(), [&](const auto &frame) { return older(frame.inputs.prepared->packetSequence); }) ||
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

        void RendererRuntime::Discard(const EncodingFrame &frame, const FrameError &error)
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
                if (drainCpu)
                    detail::DrainRuntimeCompletions(coordinator, *events);
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
