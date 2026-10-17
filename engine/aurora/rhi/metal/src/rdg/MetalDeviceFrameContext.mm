#include <rdg/MetalDeviceFrameContext.h>

#include <MetalBlitHelper.h>
#include <MetalBuffer.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>
#include <MetalEncoder.h>
#include <MetalImage.h>
#include <MetalPipelineState.h>
#include <MetalResourceGroup.h>
#include <MetalSampler.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <aurora/rhi/PipelineState.h>
#include <common/FrameTransientPool.h>

#include <map>
#include <variant>

namespace sky::aurora {
    namespace {
        struct CopyBufferStep {
            MetalBuffer *source            = nullptr;
            MetalBuffer *destination       = nullptr;
            uint64_t     size              = 0;
            uint64_t     sourceOffset      = 0;
            uint64_t     destinationOffset = 0;
        };
        struct CopyImageStep {
            MetalBuffer    *buffer = nullptr;
            MetalImage     *image  = nullptr;
            BufferImageCopy region;
            bool            upload = true;
        };
        struct MetalDraw {
            CounterPtr<GraphicsPipeline>                              pipeline;
            std::vector<std::pair<uint32_t, CounterPtr<ResourceGroup>>> sets;
            std::vector<BufferView>                                   vertexBuffers;
            MetalBuffer                                              *indexBuffer = nullptr;
            uint64_t                                                  indexOffset = 0;
            IndexType                                                 indexType   = IndexType::U32;
            CmdDrawLinear                                             command;
            CmdDrawIndexed                                            indexed;
            bool                                                      isIndexed = false;
        };
        struct RasterStep {
            MetalImage             *color      = nullptr;
            MetalImage             *depth      = nullptr;
            LoadOp                  load       = LoadOp::CLEAR;
            StoreOp                 store      = StoreOp::STORE;
            LoadOp                  depthLoad  = LoadOp::DONT_CARE;
            StoreOp                 depthStore = StoreOp::DONT_CARE;
            ClearValue              clear;
            ClearValue              depthClear;
            Extent2D                extent;
            std::vector<MetalDraw>  draws;
        };
        struct ResolveStep {
            MetalImage *source  = nullptr;
            MetalImage *target  = nullptr;
            Extent2D    extent;
            bool         depth   = false;
            bool         maximum = false;
        };
        using MetalStep = std::variant<CopyBufferStep, CopyImageStep, RasterStep, ResolveStep>;

        class MetalCompiledFrame final : public BackendCompiledFrame {
        public:
            CompiledFrameIdentity GetIdentity() const noexcept override
            {
                return reservation.identity;
            }

            FrameResourceBindings                   resources;
            FramePlanLease                          reservation;
            std::shared_ptr<const FrozenFrameGraph> graph;
            std::vector<uint32_t>                   livePasses;
            std::vector<MetalStep>                  steps;
            std::vector<CounterPtr<MetalImage>>     views;
        };

        struct MetalGpuPayload {
            FrameResourceBindings resources;
            std::vector<MetalStep> steps;
            std::vector<CounterPtr<MetalImage>> views;
        };

        FrameError MakeError(FrameErrorCode code, const char *message = nullptr)
        {
            return FrameError{code, message ? message : std::string{}};
        }

        MetalImage *MakeView(MetalImage *base, uint32_t baseMip, uint32_t mipCount, uint32_t baseLayer, uint32_t layerCount,
                             std::vector<CounterPtr<MetalImage>> &views)
        {
            if (!base)
                return nullptr;
            auto *view = base->CreateView(baseMip, mipCount, baseLayer, layerCount);
            if (!view)
                return nullptr;
            views.emplace_back(view);
            return view;
        }

        bool ResolveDraws(MetalDevice &device,
                          const FrameGraphData &data,
                          const FrameResourceBindings &bindings,
                          const FrameRasterPass &raster,
                          PixelFormat colorFormat,
                          SampleCount samples,
                          bool hasDepth,
                          PixelFormat depthFormat,
                          std::vector<CounterPtr<MetalImage>> &views,
                          std::vector<MetalDraw> &out)
        {
            for (const auto &source : raster.draws) {
                if (!source.shader || source.shader->GetDevice() != &device)
                    return false;
                MetalDraw draw;
                draw.command   = source.command;
                draw.indexed   = source.indexed ? source.indexed->command : CmdDrawIndexed{};
                draw.isIndexed = source.indexed.has_value();

                PipelineState state = RasterPipelineState(source);
                AttachmentFormat format;
                format.colors[0]    = colorFormat;
                format.numColors    = 1;
                format.sampleCount  = samples;
                format.depthStencil = hasDepth ? depthFormat : PixelFormat::UNDEFINED;
                GraphicsPipeline::Descriptor pipelineDesc{&state, source.shader.Get(), format};
                draw.pipeline = CounterPtr<GraphicsPipeline>(device.CreatePipelineState(pipelineDesc));
                if (!draw.pipeline)
                    return false;

                std::map<uint32_t, CounterPtr<ResourceGroup>> groups;
                auto groupFor = [&](uint32_t set) -> bool {
                    auto found = groups.find(set);
                    if (found != groups.end())
                        return true;
                    ResourceGroup::Descriptor groupDesc{source.shader.Get(), set};
                    CounterPtr<ResourceGroup> group(device.CreateResourceGroup(groupDesc));
                    if (!group)
                        return false;
                    auto encoder = group->CreateEncoder();
                    if (!encoder)
                        return false;
                    for (const auto &uniform : source.uniforms)
                        if (uniform.set == set)
                            encoder->WriteBuffer(uniform.binding, bindings.buffers[uniform.range.buffer.index].Get(), uniform.range.offset,
                                                 uniform.range.size, 0);
                    for (const auto &image : source.images)
                        if (image.set == set) {
                            auto *base = static_cast<MetalImage *>(bindings.images[image.image.index].Get());
                            const auto &desc = data.images[image.image.index].descriptor;
                            const auto mipCount = image.view.mipCount ? image.view.mipCount : desc.mipLevels - image.view.baseMip;
                            auto *sampled = (image.view.baseMip || mipCount != desc.mipLevels || image.view.baseLayer || image.view.layerCount != 1)
                                                ? MakeView(base, image.view.baseMip, mipCount, image.view.baseLayer, image.view.layerCount, views)
                                                : base;
                            if (!sampled)
                                return false;
                            encoder->WriteImage(image.binding, sampled, ImageLayout::SHADER_READ_ONLY, 0);
                        }
                    for (const auto &sampler : source.samplers)
                        if (sampler.set == set)
                            encoder->WriteSampler(sampler.binding, sampler.sampler.Get(), 0);
                    encoder->End();
                    draw.sets.emplace_back(set, group);
                    groups.emplace(set, std::move(group));
                    return true;
                };
                for (const auto &uniform : source.uniforms)
                    if (!groupFor(uniform.set))
                        return false;
                for (const auto &image : source.images) {
                    if (image.view.type != ImageViewType::VIEW_2D && image.view.type != ImageViewType::VIEW_2D_ARRAY)
                        return false;
                    if (!groupFor(image.set))
                        return false;
                }
                for (const auto &sampler : source.samplers)
                    if (!groupFor(sampler.set))
                        return false;

                for (const auto &stream : source.vertices) {
                    auto *buffer = static_cast<MetalBuffer *>(bindings.buffers[stream.range.buffer.index].Get());
                    draw.vertexBuffers.push_back({buffer, stream.range.offset, stream.range.size});
                }
                if (source.indexed) {
                    draw.indexBuffer = static_cast<MetalBuffer *>(bindings.buffers[source.indexed->range.buffer.index].Get());
                    draw.indexOffset = source.indexed->range.offset;
                    draw.indexType   = source.indexed->type;
                }
                out.push_back(std::move(draw));
            }
            return true;
        }
    } // namespace

    FrameBufferStorageResult MetalDeviceFrameContext::CreateFrameBuffer(const Buffer::Descriptor &desc)
    {
        auto buffer = std::make_unique<MetalBuffer>(*device);
        if (!buffer->Init(desc))
            return MakeError(FrameErrorCode::OUT_OF_MEMORY, "Metal frame buffer allocation failed");
        uint8_t *mapped = nullptr;
        if (desc.memory != MemoryType::GPU_ONLY) {
            mapped = buffer->Map();
            if (!mapped)
                return MakeError(FrameErrorCode::UNSUPPORTED, "Metal frame buffer is not host-visible");
        }
        return FrameBufferStorage{BufferPtr(buffer.release()), mapped};
    }

    FrameCompileResult MetalDeviceFrameContext::Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
    {
        auto valid = ValidateCompileLease(lease);
        if (auto *error = std::get_if<FrameError>(&valid))
            return *error;
        if (!graph)
            return MakeError(FrameErrorCode::INVALID_ARGUMENT);

        auto analysis = AnalyzeFrameGraph(*graph);
        if (auto *error = std::get_if<FrameError>(&analysis))
            return *error;
        const auto &semantic = std::get<FrameGraphAnalysis>(analysis);
        if (semantic.usedBuffers.empty() && semantic.usedImages.empty())
            return FrameNoWork{};

        auto acquired = device->GetTransientPool().Acquire(graph->GetData(), semantic, lease.context.deviceEpoch);
        if (auto *error = std::get_if<FrameError>(&acquired))
            return *error;
        auto bindings = std::move(std::get<FrameResourceBindings>(acquired));
        const auto &data = graph->GetData();

        for (auto index : semantic.usedBuffers) {
            auto *buffer = bindings.buffers[index].Get();
            if (!buffer || buffer->GetDevice() != device || buffer->GetSize() != data.buffers[index].size)
                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal frame buffer owner or size mismatch");
        }
        for (auto index : semantic.usedImages) {
            auto *image = bindings.images[index].Get();
            if (!image || image->GetDevice() != device)
                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal frame image owner mismatch");
        }

        auto plan = std::make_unique<MetalCompiledFrame>();
        plan->livePasses = semantic.livePasses;
        for (auto index : semantic.livePasses) {
            const auto &pass = data.passes[index];
            if (const auto *copy = std::get_if<BufferCopyPass>(&pass)) {
                auto *source      = static_cast<MetalBuffer *>(bindings.buffers[copy->source.index].Get());
                auto *destination = static_cast<MetalBuffer *>(bindings.buffers[copy->destination.index].Get());
                plan->steps.emplace_back(CopyBufferStep{source, destination, copy->size, copy->sourceOffset, copy->destinationOffset});
            } else if (const auto *copy = std::get_if<BufferImageCopyPass>(&pass)) {
                auto *buffer      = static_cast<MetalBuffer *>(bindings.buffers[copy->buffer.index].Get());
                auto *image       = static_cast<MetalImage *>(bindings.images[copy->image.index].Get());
                const bool upload = copy->direction == ImageCopyDirection::BUFFER_TO_IMAGE;
                plan->steps.emplace_back(CopyImageStep{buffer, image, copy->region, upload});
            } else if (const auto *raster = std::get_if<FrameRasterPass>(&pass)) {
                if (raster->colorLocalRead)
                    return MakeError(FrameErrorCode::UNSUPPORTED, "Metal raster color local read is not implemented yet");
                auto       *image = static_cast<MetalImage *>(bindings.images[raster->color.index].Get());
                const auto &desc  = data.images[raster->color.index].descriptor;
                auto       *colorView = (raster->colorView.mip || raster->colorView.layer)
                                            ? MakeView(image, raster->colorView.mip, 1, raster->colorView.layer, 1, plan->views)
                                            : image;
                if (!colorView)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal color view creation failed");
                RasterStep  step;
                step.color  = colorView;
                step.load   = raster->load;
                step.store  = raster->store;
                step.clear  = raster->clear;
                step.extent = {std::max(1u, desc.extent.width >> raster->colorView.mip),
                               std::max(1u, desc.extent.height >> raster->colorView.mip)};
                if (raster->depth) {
                    auto *depthBase = static_cast<MetalImage *>(bindings.images[raster->depth->image.index].Get());
                    step.depth      = (raster->depth->view.mip || raster->depth->view.layer)
                                          ? MakeView(depthBase, raster->depth->view.mip, 1, raster->depth->view.layer, 1, plan->views)
                                          : depthBase;
                    if (!step.depth)
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal depth view creation failed");
                    step.depthLoad  = raster->depth->load;
                    step.depthStore = raster->depth->store;
                    step.depthClear = ClearValue(0.f, 0.f, 0.f, 0.f);
                    step.depthClear.depthStencil.depth = raster->depth->clear;
                }
                if (!ResolveDraws(*device, data, bindings, *raster, desc.format, desc.samples, raster->depth.has_value(),
                                  raster->depth ? data.images[raster->depth->image.index].descriptor.format : PixelFormat::UNDEFINED,
                                  plan->views, step.draws))
                    return MakeError(FrameErrorCode::UNSUPPORTED, "Metal raster draw/pipeline binding is not supported");
                plan->steps.emplace_back(std::move(step));
                if (raster->colorResolve) {
                    ResolveStep resolve;
                    resolve.source = colorView;
                    auto *resolveBase = static_cast<MetalImage *>(bindings.images[raster->colorResolve->image.index].Get());
                    resolve.target = (raster->colorResolve->view.mip || raster->colorResolve->view.layer)
                                         ? MakeView(resolveBase, raster->colorResolve->view.mip, 1, raster->colorResolve->view.layer, 1, plan->views)
                                         : resolveBase;
                    resolve.extent = step.extent;
                    plan->steps.emplace_back(resolve);
                }
                if (raster->depth && raster->depth->resolve) {
                    ResolveStep resolve;
                    resolve.source  = step.depth;
                    resolve.depth   = true;
                    resolve.maximum = raster->depth->resolve->mode == DepthResolveMode::MAXIMUM;
                    auto *resolveBase = static_cast<MetalImage *>(bindings.images[raster->depth->resolve->image.index].Get());
                    resolve.target = (raster->depth->resolve->view.mip || raster->depth->resolve->view.layer)
                                         ? MakeView(resolveBase, raster->depth->resolve->view.mip, 1, raster->depth->resolve->view.layer, 1, plan->views)
                                         : resolveBase;
                    resolve.extent = step.extent;
                    plan->steps.emplace_back(resolve);
                }
            } else {
                return MakeError(FrameErrorCode::UNSUPPORTED, "Metal compute/custom passes are not implemented yet");
            }
        }

        auto reserved = ReservePlan(lease, *graph, bindings);
        if (auto *error = std::get_if<FrameError>(&reserved))
            return *error;
        plan->reservation = std::move(std::get<FramePlanLease>(reserved));
        plan->resources   = std::move(bindings);
        plan->graph       = std::move(graph);
        return std::unique_ptr<BackendCompiledFrame>(std::move(plan));
    }

    FrameDispatchResult MetalDeviceFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &compiled)
    {
        if (!compiled)
            return MakeError(FrameErrorCode::INVALID_ARGUMENT);
        auto *plan = static_cast<MetalCompiledFrame *>(compiled.get());
        if (plan->reservation.identity.lease.context != GetIdentity())
            return MakeError(FrameErrorCode::WRONG_CONTEXT);

        auto started = BeginEncoding(plan->reservation);
        if (auto *error = std::get_if<FrameError>(&started))
            return *error;
        const auto ticket = std::get<EncodingTicket>(started);

        auto allocation = AllocatePlanCommand(plan->reservation, 0);
        if (auto *error = std::get_if<FrameError>(&allocation)) {
            PublishEncoding(ticket, *error);
            return ticket;
        }
        auto *command = static_cast<MetalCommandBuffer *>(std::get<CommandBuffer *>(allocation));
        command->Begin();
        for (const auto &step : plan->steps) {
            if (const auto *copy = std::get_if<CopyBufferStep>(&step)) {
                auto encoder = command->CreateBlitEncoder();
                encoder->CopyBuffer(copy->source, copy->destination, copy->size, copy->sourceOffset, copy->destinationOffset);
            } else if (const auto *copy = std::get_if<CopyImageStep>(&step)) {
                auto encoder = command->CreateBlitEncoder();
                if (copy->upload)
                    encoder->CopyBufferToImage(copy->buffer, copy->image, {copy->region});
                else
                    encoder->CopyImageToBuffer(copy->image, copy->buffer, {copy->region});
            } else if (const auto *rasterPtr = std::get_if<RasterStep>(&step)) {
                const auto &raster  = *rasterPtr;
                auto        encoder = command->CreateGraphicsEncoder();
                RenderingInfo info;
                info.renderArea           = {{0, 0}, raster.extent};
                info.numColors            = 1;
                info.colors[0].image      = raster.color;
                info.colors[0].loadOp     = raster.load;
                info.colors[0].storeOp    = raster.store;
                info.colors[0].clearValue = raster.clear;
                if (raster.depth) {
                    info.depthStencil.image          = raster.depth;
                    info.depthStencil.depthLoadOp    = raster.depthLoad;
                    info.depthStencil.depthStoreOp   = raster.depthStore;
                    info.depthStencil.clearValue     = raster.depthClear;
                }
                encoder->BeginRendering(info);
                for (const auto &draw : raster.draws) {
                    encoder->BindPipeline(draw.pipeline.Get());
                    for (const auto &[set, group] : draw.sets)
                        encoder->BindResourceGroup(set, group.Get(), 0, nullptr);
                    if (!draw.vertexBuffers.empty())
                        encoder->BindVertexBuffers(0, static_cast<uint32_t>(draw.vertexBuffers.size()), draw.vertexBuffers.data());
                    if (draw.isIndexed) {
                        encoder->BindIndexBuffer(draw.indexBuffer, draw.indexOffset, draw.indexType);
                        encoder->DrawIndexed(draw.indexed);
                    } else {
                        encoder->Draw(draw.command);
                    }
                }
                encoder->EndRendering();
            } else {
                const auto &resolve = std::get<ResolveStep>(step);
                if (resolve.depth) {
                    device->GetBlitHelper()->ResolveDepth(command->GetNativeHandle(), resolve.source, resolve.target, resolve.maximum);
                } else {
                    auto        encoder = command->CreateBlitEncoder();
                    ResolveInfo info;
                    info.srcRange.layers = 1;
                    info.dstRange.layers = 1;
                    info.srcRange.aspectMask = AspectFlagBit::COLOR_BIT;
                    info.dstRange.aspectMask = AspectFlagBit::COLOR_BIT;
                    info.extent = {resolve.extent.width, resolve.extent.height, 1};
                    encoder->ResolveImage(resolve.source, resolve.target, {info});
                }
            }
        }
        command->End();

        FrameRecordingInputs inputs;
        inputs.receipt  = {plan->graph, plan->livePasses};
        inputs.payload  = std::make_shared<const MetalGpuPayload>(
            MetalGpuPayload{std::move(plan->resources), std::move(plan->steps), std::move(plan->views)});
        PublishEncoding(ticket, FinishPlanRecording(plan->reservation, std::move(inputs)));
        return ticket;
    }

} // namespace sky::aurora
