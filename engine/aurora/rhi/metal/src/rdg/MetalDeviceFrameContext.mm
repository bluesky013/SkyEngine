#include <rdg/MetalDeviceFrameContext.h>

#include "MetalFormat.h"

#include <Metal/Metal.h>
#include <MetalBlitHelper.h>
#include <MetalBuffer.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>
#include <MetalEncoder.h>
#include <MetalImage.h>
#include <MetalPipelineState.h>
#include <MetalResourceGroup.h>
#include <MetalSampler.h>
#include <MetalShader.h>
#include <aurora/rdg/FrameBindingLookup.h>
#include <aurora/rdg/FrozenFrameGraph.h>
#include <aurora/rhi/DescriptorEncoder.h>
#include <aurora/rhi/PipelineState.h>
#include <common/FrameTransientPool.h>

#include <algorithm>
#include <map>
#include <tuple>
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
        struct SampledDepthCopyStep {
            MetalImage *source      = nullptr;
            MetalImage *destination = nullptr;
            uint32_t    baseMip = 0, mipCount = 1, baseLayer = 0, layerCount = 1;
            Extent2D    extent;
        };
        struct MetalSet {
            uint32_t                  set = 0;
            CounterPtr<ResourceGroup> group;
            std::vector<uint32_t>     dynamicOffsets;
        };
        struct MetalDraw {
            CounterPtr<GraphicsPipeline> pipeline;
            std::vector<MetalSet>        sets;
            std::vector<BufferView>      vertexBuffers;
            MetalBuffer                 *indexBuffer = nullptr;
            uint64_t                     indexOffset = 0;
            IndexType                    indexType   = IndexType::U32;
            CmdDrawLinear                command;
            CmdDrawIndexed               indexed;
            Rect2D                       scissor;
            bool                         isIndexed = false;
        };
        struct RasterStep {
            MetalImage            *color      = nullptr;
            MetalImage            *depth      = nullptr;
            LoadOp                 load       = LoadOp::CLEAR;
            StoreOp                store      = StoreOp::STORE;
            LoadOp                 depthLoad  = LoadOp::DONT_CARE;
            StoreOp                depthStore = StoreOp::DONT_CARE;
            ClearValue             clear;
            ClearValue             depthClear;
            Extent2D               extent;
            std::vector<MetalDraw> draws;
        };
        struct ResolveStep {
            MetalImage *source = nullptr;
            MetalImage *target = nullptr;
            Extent2D    extent;
            bool        depth   = false;
            bool        maximum = false;
        };
        struct ComputeStep {
            CounterPtr<ComputePipeline> pipeline;
            std::vector<MetalSet>       sets;
            uint32_t                    groupsX = 1, groupsY = 1, groupsZ = 1;
        };
        struct CustomStep {
            std::shared_ptr<const FrameCustomPassEncoder> encoder;
            std::vector<Image *>                          images;
            std::vector<Buffer *>                         buffers;
        };
        using MetalStep = std::variant<CopyBufferStep, CopyImageStep, SampledDepthCopyStep, RasterStep, ResolveStep, ComputeStep, CustomStep>;

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
            FrameResourceBindings               resources;
            std::vector<MetalStep>              steps;
            std::vector<CounterPtr<MetalImage>> views;
        };

        FrameError MakeError(FrameErrorCode code, const char *message = nullptr)
        {
            return FrameError{code, message ? message : std::string{}};
        }

        std::optional<FrameError> ValidateComputeInputs(MetalDevice &device, const FrameComputePass &pass, const FrameResourceBindings &bindings)
        {
            if (!pass.shader || pass.shader->GetDevice() != &device)
                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute shader owner mismatch");
            const auto *shader = static_cast<const MetalShader *>(pass.shader.Get());
            if (!shader->GetComputeFunction() || shader->GetComputeFunction()->GetStage() != ShaderStageFlagBit::CS || shader->GetVertexFunction() ||
                shader->GetFragmentFunction())
                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Compute pass requires a compute-only shader");
            const auto                              &reflection = shader->GetReflection();
            const FrameBindingLookup<ShaderResource> resourceBindings(reflection.resources);
            const auto                               nativeDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
            if (!nativeDevice)
                return MakeError(FrameErrorCode::UNSUPPORTED, "Metal compute device is unavailable");
            const auto       maximum  = nativeDevice.maxThreadsPerThreadgroup;
            const NSUInteger limits[] = {maximum.width, maximum.height, maximum.depth};
            const uint32_t   groups[] = {pass.groupsX, pass.groupsY, pass.groupsZ};
            for (uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = reflection.threadGroupSize[axis];
                if (!threads)
                    return MakeError(FrameErrorCode::UNSUPPORTED, "Metal requires reflected compute threadgroup dimensions");
                if (!groups[axis] || threads > limits[axis])
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Compute threadgroup exceeds Metal device limits");
            }
            for (const auto &binding : pass.buffers) {
                const auto *resource = resourceBindings.Find(binding.set, binding.binding);
                if (!resource)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Missing compute buffer reflection");
                const auto *buffer = static_cast<const MetalBuffer *>(bindings.buffers[binding.range.buffer.index].Get());
                const auto  range  = ResolveFrameBufferRange(binding, buffer->GetSize());
                if (const auto *error = std::get_if<FrameError>(&range))
                    return *error;
                const auto &desc = buffer->GetDescriptor();
                const bool  uniform =
                    resource->type == ShaderResourceType::UNIFORM_BUFFER || resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                // Valid memory types map to GPU-accessible private/shared buffers; only unknown enum values are rejected here.
                if (!desc.usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE) || desc.memory > MemoryType::GPU_TO_CPU)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Invalid Metal compute buffer usage/memory");
                if (uniform) {
                    const auto alignment = device.GetCapability().minUniformBufferOffsetAlignment;
                    if (!alignment || binding.range.offset % alignment || binding.dynamicOffset % alignment)
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Invalid Metal compute uniform offset alignment");
                }
                const auto native = (__bridge id<MTLBuffer>)buffer->GetNativeHandle();
                if (!native)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute buffer is missing native storage");
            }
            return {};
        }

        MetalImage *MakeView(MetalImage                          *base,
                             uint32_t                             baseMip,
                             uint32_t                             mipCount,
                             uint32_t                             baseLayer,
                             uint32_t                             layerCount,
                             std::vector<CounterPtr<MetalImage>> &views,
                             ImageViewType                        type = ImageViewType::VIEW_2D)
        {
            if (!base)
                return nullptr;
            auto *view = base->CreateView(baseMip, mipCount, baseLayer, layerCount, type == ImageViewType::VIEW_2D_ARRAY);
            if (!view)
                return nullptr;
            views.emplace_back(view);
            return view;
        }

        bool ResolveDraws(MetalDevice                         &device,
                          const FrameGraphData                &data,
                          const FrameResourceBindings         &bindings,
                          const FrameRasterPass               &raster,
                          PixelFormat                          colorFormat,
                          SampleCount                          samples,
                          bool                                 hasDepth,
                          PixelFormat                          depthFormat,
                          std::vector<CounterPtr<MetalImage>> &views,
                          std::vector<MetalStep>              &steps,
                          std::vector<MetalDraw>              &out)
        {
            std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t, uint32_t>, MetalImage *> sampledDepthCopies;
            for (const auto &source : raster.draws) {
                if (!source.shader || source.shader->GetDevice() != &device)
                    return false;
                for (const auto &sampler : source.samplers)
                    if (!sampler.sampler || sampler.sampler->GetDevice() != &device)
                        return false;
                const auto alignment = device.GetCapability().minUniformBufferOffsetAlignment;
                for (const auto &uniform : source.uniforms)
                    if (!alignment || uniform.range.offset % alignment || uniform.dynamicOffset % alignment)
                        return false;
                MetalDraw draw;
                draw.command   = source.command;
                draw.indexed   = source.indexed ? source.indexed->command : CmdDrawIndexed{};
                draw.isIndexed = source.indexed.has_value();
                draw.scissor   = source.scissor;

                PipelineState    state = RasterPipelineState(source);
                AttachmentFormat format;
                format.colors[0] = colorFormat;
                format.numColors = colorFormat == PixelFormat::UNDEFINED ? 0 : 1;
                if (!format.numColors)
                    state.blendStates.clear();
                format.sampleCount  = samples;
                format.depthStencil = hasDepth ? depthFormat : PixelFormat::UNDEFINED;
                GraphicsPipeline::Descriptor pipelineDesc{&state, source.shader.Get(), format};
                draw.pipeline = CounterPtr<GraphicsPipeline>(device.CreatePipelineState(pipelineDesc));
                if (!draw.pipeline)
                    return false;

                std::map<uint32_t, CounterPtr<ResourceGroup>> groups;
                auto                                          groupFor = [&](uint32_t set) -> bool {
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
                        if (uniform.set == set && !encoder->WriteBuffer(uniform.binding, bindings.buffers[uniform.range.buffer.index].Get(),
                                                                                                                 uniform.range.offset, uniform.range.size, 0))
                            return false;
                    for (const auto &image : source.images)
                        if (image.set == set) {
                            auto       *base = static_cast<MetalImage *>(bindings.images[image.image.index].Get());
                            const auto &desc = data.images[image.image.index].descriptor;
                            const auto  mipCount = image.view.mipCount ? image.view.mipCount : desc.mipLevels - image.view.baseMip;
                            const bool  restricted =
                                image.view.baseMip || mipCount != desc.mipLevels || image.view.baseLayer || image.view.layerCount != desc.arrayLayers;
                            MetalImage *sampled = nullptr;
                            if (GetImageFormatInfo(desc.format).hasDepth && image.view.type == ImageViewType::VIEW_2D_ARRAY && restricted) {
                                const auto key =
                                    std::make_tuple(image.image.index, image.view.baseMip, mipCount, image.view.baseLayer, image.view.layerCount);
                                if (auto found = sampledDepthCopies.find(key); found != sampledDepthCopies.end()) {
                                    sampled = found->second;
                                } else {
                                    auto copyDesc = desc;
                                    copyDesc.extent.width = std::max(1u, desc.extent.width >> image.view.baseMip);
                                    copyDesc.extent.height = std::max(1u, desc.extent.height >> image.view.baseMip);
                                    copyDesc.mipLevels   = mipCount;
                                    copyDesc.arrayLayers = image.view.layerCount;
                                    copyDesc.usage = ImageUsageFlagBit::SAMPLED | ImageUsageFlagBit::TRANSFER_DST;
                                    copyDesc.viewUsage = {};
                                    copyDesc.memory    = MemoryType::GPU_ONLY;
                                    auto copy          = std::make_unique<MetalImage>(device);
                                    if (desc.samples != SampleCount::X1 || !copy->Init(copyDesc, true))
                                        return false;
                                    sampled = copy.release();
                                    views.emplace_back(sampled);
                                    sampledDepthCopies.emplace(key, sampled);
                                    // Restricted depth-array mip views can sample the wrong parent mip on Intel Metal.
                                    steps.emplace_back(SampledDepthCopyStep{base,
                                                                            sampled,
                                                                            image.view.baseMip,
                                                                            mipCount,
                                                                            image.view.baseLayer,
                                                                            image.view.layerCount,
                                                                                                                     {copyDesc.extent.width, copyDesc.extent.height}});
                                }
                            } else {
                                sampled = (restricted || image.view.layerCount != desc.arrayLayers ||
                                           (image.view.type == ImageViewType::VIEW_2D_ARRAY && desc.arrayLayers == 1))
                                                                                       ? MakeView(base, image.view.baseMip, mipCount, image.view.baseLayer, image.view.layerCount, views,
                                                                                                  image.view.type)
                                                                                       : base;
                            }
                            if (!sampled)
                                return false;
                            if (!encoder->WriteImage(image.binding, sampled, ImageLayout::SHADER_READ_ONLY, 0))
                                return false;
                        }
                    for (const auto &sampler : source.samplers)
                        if (sampler.set == set && !encoder->WriteSampler(sampler.binding, sampler.sampler.Get(), 0))
                            return false;
                    if (!encoder->End())
                        return false;
                    std::vector<std::pair<uint32_t, uint32_t>> offsets;
                    for (const auto &resource : source.shader->GetReflection().resources) {
                        if (resource.set != set || resource.type != ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
                            continue;
                        const auto binding = std::find_if(source.uniforms.begin(), source.uniforms.end(),
                                                                                                   [&](const auto &value) { return value.set == set && value.binding == resource.binding; });
                        if (binding == source.uniforms.end())
                            return false;
                        offsets.emplace_back(resource.binding, binding->dynamicOffset);
                    }
                    std::sort(offsets.begin(), offsets.end());
                    MetalSet nativeSet{set, group, {}};
                    for (const auto &[binding, offset] : offsets)
                        nativeSet.dynamicOffsets.push_back(offset);
                    draw.sets.push_back(std::move(nativeSet));
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

    namespace {
        class MetalPassCompiler final {
        public:
            MetalPassCompiler(MetalDevice *device, const FrameGraphData &data, const FrameResourceBindings &bindings, MetalCompiledFrame &plan)
                : device(device), data(data), bindings(bindings), plan(plan)
            {
            }
            std::optional<FrameError> Compile(const BufferCopyPass &copy)
            {
                auto *source      = static_cast<MetalBuffer *>(bindings.buffers[copy.source.index].Get());
                auto *destination = static_cast<MetalBuffer *>(bindings.buffers[copy.destination.index].Get());
                plan.steps.emplace_back(CopyBufferStep{source, destination, copy.size, copy.sourceOffset, copy.destinationOffset});

                return {};
            }
            std::optional<FrameError> Compile(const BufferImageCopyPass &copy)
            {
                auto      *buffer = static_cast<MetalBuffer *>(bindings.buffers[copy.buffer.index].Get());
                auto      *image  = static_cast<MetalImage *>(bindings.images[copy.image.index].Get());
                const bool upload = copy.direction == ImageCopyDirection::BUFFER_TO_IMAGE;
                plan.steps.emplace_back(CopyImageStep{buffer, image, copy.region, upload});

                return {};
            }
            std::optional<FrameError> Compile(const FrameRasterPass &raster)
            {
                if (raster.colorLocalRead)
                    return MakeError(FrameErrorCode::UNSUPPORTED, "Metal raster color local read is not implemented yet");
                const bool  hasColor       = raster.color != GraphImageHandle{};
                const auto  attachment     = hasColor ? raster.color : raster.depth->image;
                const auto  attachmentView = hasColor ? raster.colorView : raster.depth->view;
                auto       *image          = static_cast<MetalImage *>(bindings.images[attachment.index].Get());
                const auto &desc           = data.images[attachment.index].descriptor;
                auto       *colorView      = !hasColor ? nullptr
                                             : (raster.colorView.mip || raster.colorView.layer)
                                                 ? MakeView(image, raster.colorView.mip, 1, raster.colorView.layer, 1, plan.views)
                                                 : image;
                if (hasColor && !colorView)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal color view creation failed");
                RasterStep step;
                step.color  = colorView;
                step.load   = raster.load;
                step.store  = raster.store;
                step.clear  = raster.clear;
                step.extent = {std::max(1u, desc.extent.width >> attachmentView.mip), std::max(1u, desc.extent.height >> attachmentView.mip)};
                if (raster.depth) {
                    auto *depthBase = static_cast<MetalImage *>(bindings.images[raster.depth->image.index].Get());
                    step.depth      = (raster.depth->view.mip || raster.depth->view.layer)
                                          ? MakeView(depthBase, raster.depth->view.mip, 1, raster.depth->view.layer, 1, plan.views)
                                          : depthBase;
                    if (!step.depth)
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal depth view creation failed");
                    step.depthLoad                     = raster.depth->load;
                    step.depthStore                    = raster.depth->store;
                    step.depthClear                    = ClearValue(0.f, 0.f, 0.f, 0.f);
                    step.depthClear.depthStencil.depth = raster.depth->clear;
                }
                if (!ResolveDraws(*device, data, bindings, raster, hasColor ? desc.format : PixelFormat::UNDEFINED, desc.samples,
                                  raster.depth.has_value(),
                                  raster.depth ? data.images[raster.depth->image.index].descriptor.format : PixelFormat::UNDEFINED, plan.views,
                                  plan.steps, step.draws))
                    return MakeError(FrameErrorCode::UNSUPPORTED, "Metal raster draw/pipeline binding is not supported");
                plan.steps.emplace_back(std::move(step));
                if (raster.colorResolve) {
                    ResolveStep resolve;
                    resolve.source    = colorView;
                    auto *resolveBase = static_cast<MetalImage *>(bindings.images[raster.colorResolve->image.index].Get());
                    resolve.target    = (raster.colorResolve->view.mip || raster.colorResolve->view.layer)
                                            ? MakeView(resolveBase, raster.colorResolve->view.mip, 1, raster.colorResolve->view.layer, 1, plan.views)
                                            : resolveBase;
                    resolve.extent    = step.extent;
                    plan.steps.emplace_back(resolve);
                }
                if (raster.depth && raster.depth->resolve) {
                    ResolveStep resolve;
                    resolve.source    = step.depth;
                    resolve.depth     = true;
                    resolve.maximum   = raster.depth->resolve->mode == DepthResolveMode::MAXIMUM;
                    auto *resolveBase = static_cast<MetalImage *>(bindings.images[raster.depth->resolve->image.index].Get());
                    resolve.target    = (raster.depth->resolve->view.mip || raster.depth->resolve->view.layer)
                                            ? MakeView(resolveBase, raster.depth->resolve->view.mip, 1, raster.depth->resolve->view.layer, 1, plan.views)
                                            : resolveBase;
                    resolve.extent    = step.extent;
                    plan.steps.emplace_back(resolve);
                }

                return {};
            }
            std::optional<FrameError> Compile(const FrameComputePass &compute)
            {
                if (const auto error = ValidateComputeInputs(*device, compute, bindings))
                    return *error;
                ComputeStep step;
                step.groupsX = compute.groupsX;
                step.groupsY = compute.groupsY;
                step.groupsZ = compute.groupsZ;
                ComputePipeline::Descriptor pipelineDesc{compute.shader.Get()};
                step.pipeline = CounterPtr<ComputePipeline>(device->CreatePipelineState(pipelineDesc));
                if (!step.pipeline)
                    return MakeError(FrameErrorCode::COMPILE_FAILED, "Metal compute pipeline creation failed");
                const auto                              *pipeline       = static_cast<const MetalComputePipeline *>(step.pipeline.Get());
                const auto                               nativePipeline = (__bridge id<MTLComputePipelineState>)pipeline->GetNativeHandle();
                const auto                              &reflection     = compute.shader->GetReflection();
                const FrameBindingLookup<ShaderResource> resourceBindings(reflection.resources);
                uint64_t                                 threads = 1;
                for (const auto size : reflection.threadGroupSize) {
                    if (size > nativePipeline.maxTotalThreadsPerThreadgroup / threads)
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Compute threadgroup exceeds Metal pipeline limits");
                    threads *= size;
                }
                for (uint32_t set = 0; set < 3; ++set) {
                    if (!resourceBindings.HasSet(set))
                        continue;
                    const auto *setPlan = static_cast<const MetalShader *>(compute.shader.Get())->GetResourceSetPlan(set);
                    if (setPlan == nullptr)
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Missing linked Metal resource set");
                    const auto &slots = setPlan->stages[2];
                    for (const auto &binding : compute.buffers) {
                        if (binding.set != set)
                            continue;
                        const auto slot = slots.buffer.find(binding.binding);
                        if (slot == slots.buffer.end())
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Missing Metal compute buffer slot");
                        const auto alignment = pipeline->GetBufferAlignment(slot->second);
                        if (!alignment)
                            return MakeError(FrameErrorCode::UNSUPPORTED, "Metal compute buffer alignment reflection unavailable");
                        if (*alignment && (binding.range.offset % *alignment || binding.dynamicOffset % *alignment))
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute buffer offset violates native alignment");
                    }
                    ResourceGroup::Descriptor groupDesc{compute.shader.Get(), set};
                    CounterPtr<ResourceGroup> group(device->CreateResourceGroup(groupDesc));
                    if (!group)
                        return MakeError(FrameErrorCode::UNSUPPORTED, "Metal compute resource group creation failed");
                    auto encoder = group->CreateEncoder();
                    if (!encoder)
                        return MakeError(FrameErrorCode::OUT_OF_MEMORY);
                    for (const auto &binding : compute.buffers)
                        if (binding.set == set && !encoder->WriteBuffer(binding.binding, bindings.buffers[binding.range.buffer.index].Get(),
                                                                        binding.range.offset, binding.range.size, 0))
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute buffer binding failed");
                    for (const auto &binding : compute.images) {
                        if (binding.set != set)
                            continue;
                        auto       *base     = static_cast<MetalImage *>(bindings.images[binding.image.index].Get());
                        const auto &desc     = data.images[binding.image.index].descriptor;
                        const auto *resource = resourceBindings.Find(binding.set, binding.binding);
                        if (!resource)
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Missing compute image reflection");
                        const bool storage  = resource->type == ShaderResourceType::STORAGE_IMAGE;
                        const auto resolved = ResolveFrameImageView(binding, desc, storage);
                        if (const auto *error = std::get_if<FrameError>(&resolved))
                            return *error;
                        const auto &range = std::get<RasterImageView>(resolved);
                        auto *view = (range.baseMip || range.mipCount != desc.mipLevels || range.baseLayer || range.layerCount != desc.arrayLayers ||
                                      (range.type == ImageViewType::VIEW_2D_ARRAY && desc.arrayLayers == 1))
                                         ? MakeView(base, range.baseMip, range.mipCount, range.baseLayer, range.layerCount, plan.views, range.type)
                                         : base;
                        if (!view)
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute image view creation failed");
                        if (!encoder->WriteImage(binding.binding, view, storage ? ImageLayout::GENERAL : ImageLayout::SHADER_READ_ONLY, 0))
                            return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute image binding failed");
                    }
                    for (const auto &binding : compute.samplers)
                        if (binding.set == set) {
                            if (binding.sampler->GetDevice() != device)
                                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute sampler owner mismatch");
                            if (!encoder->WriteSampler(binding.binding, binding.sampler.Get(), 0))
                                return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute sampler binding failed");
                        }
                    if (!encoder->End())
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal compute descriptor commit failed");
                    step.sets.push_back({set, group, ResolveComputeDynamicOffsets(compute, set)});
                }
                plan.steps.emplace_back(std::move(step));

                return {};
            }
            std::optional<FrameError> Compile(const FrameCustomPass &custom)
            {
                CustomStep step;
                step.encoder = custom.encoder;
                for (const auto &binding : custom.buffers) {
                    const auto *buffer  = static_cast<MetalBuffer *>(bindings.buffers[binding.range.buffer.index].Get());
                    const bool  uniform = binding.kind == FrameBufferBindingKind::UNIFORM;
                    if (!buffer->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE))
                        return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Custom buffer kind does not match usage");
                    step.buffers.push_back(bindings.buffers[binding.range.buffer.index].Get());
                }
                for (const auto &binding : custom.images)
                    step.images.push_back(bindings.images[binding.image.index].Get());
                plan.steps.emplace_back(std::move(step));

                return {};
            }

        private:
            MetalDevice                 *device;
            const FrameGraphData        &data;
            const FrameResourceBindings &bindings;
            MetalCompiledFrame          &plan;
        };
    } // namespace
    FrameCompileResult MetalDeviceFrameContext::Compile(const DeviceFrameLease &lease, std::shared_ptr<const FrozenFrameGraph> graph)
    {
        @autoreleasepool {
            auto valid = ValidateCompileLease(lease);
            if (auto *error = std::get_if<FrameError>(&valid))
                return *error;
            if (!graph)
                return MakeError(FrameErrorCode::INVALID_ARGUMENT);

            auto analysis = AnalyzeFrameGraph(*graph);
            if (auto *error = std::get_if<FrameError>(&analysis))
                return *error;
            const auto &semantic = std::get<FrameGraphAnalysis>(analysis);
            if (semantic.livePasses.empty() && semantic.usedBuffers.empty() && semantic.usedImages.empty())
                return FrameNoWork{};

            auto acquired = device->GetTransientPool().Acquire(graph->GetData(), semantic, lease.context.deviceEpoch);
            if (auto *error = std::get_if<FrameError>(&acquired))
                return *error;
            auto        bindings = std::move(std::get<FrameResourceBindings>(acquired));
            const auto &data     = graph->GetData();

            for (auto index : semantic.usedBuffers) {
                auto *buffer = bindings.buffers[index].Get();
                if (!buffer || buffer->GetDevice() != device || buffer->GetSize() != data.buffers[index].size)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal frame buffer owner or size mismatch");
            }
            for (auto index : semantic.usedImages) {
                auto *image = bindings.images[index].Get();
                if (!image || image->GetDevice() != device)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal frame image owner mismatch");
                const auto  native = (__bridge id<MTLTexture>)static_cast<MetalImage *>(image)->GetNativeHandle();
                const auto &desc   = data.images[index].descriptor;
                const auto  usage  = ToMetalTextureUsage(desc.usage);
                if (!native || native.pixelFormat != ToMetalPixelFormat(desc.format) || native.textureType != ToMetalTextureType(desc) ||
                    native.width != desc.extent.width || native.height != desc.extent.height || native.depth != desc.extent.depth ||
                    native.mipmapLevelCount != desc.mipLevels || native.arrayLength != desc.arrayLayers ||
                    native.sampleCount != ToMetalSampleCount(desc.samples) || (native.usage & usage) != usage)
                    return MakeError(FrameErrorCode::INVALID_ARGUMENT, "Metal frame image descriptor does not match native storage");
            }

            auto plan        = std::make_unique<MetalCompiledFrame>();
            plan->livePasses = semantic.livePasses;
            MetalPassCompiler compiler(device, data, bindings, *plan);
            for (auto index : semantic.livePasses) {
                if (auto error = std::visit([&](const auto &pass) { return compiler.Compile(pass); }, data.passes[index]))
                    return *error;
            }

            auto reserved = ReservePlan(lease, *graph, bindings);
            if (auto *error = std::get_if<FrameError>(&reserved))
                return *error;
            plan->reservation = std::move(std::get<FramePlanLease>(reserved));
            plan->resources   = std::move(bindings);
            plan->graph       = std::move(graph);
            return std::unique_ptr<BackendCompiledFrame>(std::move(plan));
        }
    }

    FrameDispatchResult MetalDeviceFrameContext::Dispatch(std::unique_ptr<BackendCompiledFrame> &compiled)
    {
        @autoreleasepool {
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
            std::optional<FrameError> encodeError;
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
                } else if (const auto *copy = std::get_if<SampledDepthCopyStep>(&step)) {
                    const auto native  = (__bridge id<MTLCommandBuffer>)command->GetNativeHandle();
                    auto       encoder = [native blitCommandEncoder];
                    if (!encoder) {
                        encodeError = MakeError(FrameErrorCode::OUT_OF_MEMORY, "Metal sampled depth copy encoder creation failed");
                        break;
                    }
                    const auto source      = (__bridge id<MTLTexture>)copy->source->GetNativeHandle();
                    const auto destination = (__bridge id<MTLTexture>)copy->destination->GetNativeHandle();
                    for (uint32_t layer = 0; layer < copy->layerCount; ++layer)
                        for (uint32_t mip = 0; mip < copy->mipCount; ++mip)
                            [encoder copyFromTexture:source
                                         sourceSlice:copy->baseLayer + layer
                                         sourceLevel:copy->baseMip + mip
                                        sourceOrigin:MTLOriginMake(0, 0, 0)
                                          sourceSize:MTLSizeMake(std::max(1u, copy->extent.width >> mip), std::max(1u, copy->extent.height >> mip), 1)
                                           toTexture:destination
                                    destinationSlice:layer
                                    destinationLevel:mip
                                   destinationOrigin:MTLOriginMake(0, 0, 0)];
                    [encoder endEncoding];
                } else if (const auto *rasterPtr = std::get_if<RasterStep>(&step)) {
                    const auto   &raster  = *rasterPtr;
                    auto          encoder = command->CreateGraphicsEncoder();
                    RenderingInfo info;
                    info.renderArea           = {{0, 0}, raster.extent};
                    info.numColors            = raster.color ? 1 : 0;
                    info.colors[0].image      = raster.color;
                    info.colors[0].loadOp     = raster.load;
                    info.colors[0].storeOp    = raster.store;
                    info.colors[0].clearValue = raster.clear;
                    if (raster.depth) {
                        info.depthStencil.image        = raster.depth;
                        info.depthStencil.depthLoadOp  = raster.depthLoad;
                        info.depthStencil.depthStoreOp = raster.depthStore;
                        info.depthStencil.clearValue   = raster.depthClear;
                    }
                    encoder->BeginRendering(info);
                    for (const auto &draw : raster.draws) {
                        encoder->BindPipeline(draw.pipeline.Get());
                        const auto scissor = draw.scissor.extent.width && draw.scissor.extent.height ? draw.scissor : Rect2D{{0, 0}, raster.extent};
                        encoder->SetScissor(1, &scissor);
                        for (const auto &set : draw.sets)
                            encoder->BindResourceGroup(set.set, set.group.Get(), static_cast<uint32_t>(set.dynamicOffsets.size()),
                                                       set.dynamicOffsets.data());
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
                } else if (const auto *compute = std::get_if<ComputeStep>(&step)) {
                    auto encoder = command->CreateComputeEncoder();
                    encoder->BindPipeline(compute->pipeline.Get());
                    for (const auto &set : compute->sets)
                        encoder->BindResourceGroup(set.set, set.group.Get(), static_cast<uint32_t>(set.dynamicOffsets.size()),
                                                   set.dynamicOffsets.data());
                    encoder->Dispatch(compute->groupsX, compute->groupsY, compute->groupsZ);
                } else if (const auto *custom = std::get_if<CustomStep>(&step)) {
                    FrameCustomPassContext context{*command, std::span<Image *const>(custom->images), std::span<Buffer *const>(custom->buffers)};
                    if (auto result = custom->encoder->Encode(context); auto *error = std::get_if<FrameError>(&result)) {
                        encodeError = std::move(*error);
                        break;
                    }
                } else {
                    const auto &resolve = std::get<ResolveStep>(step);
                    if (resolve.depth) {
                        device->GetBlitHelper()->ResolveDepth(command->GetNativeHandle(), resolve.source, resolve.target, resolve.maximum);
                    } else {
                        auto        encoder = command->CreateBlitEncoder();
                        ResolveInfo info;
                        info.srcRange.layers     = 1;
                        info.dstRange.layers     = 1;
                        info.srcRange.aspectMask = AspectFlagBit::COLOR_BIT;
                        info.dstRange.aspectMask = AspectFlagBit::COLOR_BIT;
                        info.extent              = {resolve.extent.width, resolve.extent.height, 1};
                        encoder->ResolveImage(resolve.source, resolve.target, {info});
                    }
                }
            }
            command->End();
            if (encodeError) {
                PublishEncoding(ticket, std::move(*encodeError));
                return ticket;
            }

            FrameRecordingInputs inputs;
            inputs.receipt = {plan->graph, plan->livePasses};
            inputs.payload =
                std::make_shared<const MetalGpuPayload>(MetalGpuPayload{std::move(plan->resources), std::move(plan->steps), std::move(plan->views)});
            PublishEncoding(ticket, FinishPlanRecording(plan->reservation, std::move(inputs)));
            return ticket;
        }
    }

} // namespace sky::aurora
