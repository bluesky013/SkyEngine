//
// Created on 2026/10/15.
//

#include "D3D12FrameCompiler.h"

#include <D3D12Conversion.h>
#include <D3D12Device.h>
#include <D3D12Sampler.h>
#include <algorithm>
#include <map>
#include <rdg/D3D12DeviceFrameContext.h>

namespace sky::aurora {
    namespace {
        D3D12_RESOURCE_BARRIER Transition(ID3D12Resource       *resource,
                                          D3D12_RESOURCE_STATES before,
                                          D3D12_RESOURCE_STATES after,
                                          uint32_t              subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type       = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition = {resource, subresource, before, after};
            return barrier;
        }
        D3D12_RESOURCE_STATES BufferState(CopyBufferState state)
        {
            switch (state) {
            case CopyBufferState::HOST_WRITE: return D3D12_RESOURCE_STATE_GENERIC_READ;
            case CopyBufferState::COPY_SOURCE: return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case CopyBufferState::RASTER_READ: return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER;
            case CopyBufferState::HOST_READ:
            case CopyBufferState::COPY_DESTINATION: return D3D12_RESOURCE_STATE_COPY_DEST;
            default: return D3D12_RESOURCE_STATE_COMMON;
            }
        }
        D3D12_RESOURCE_STATES ImageState(ImageLayout state)
        {
            switch (state) {
            case ImageLayout::SHADER_READ_ONLY: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            case ImageLayout::COLOR_ATTACHMENT: return D3D12_RESOURCE_STATE_RENDER_TARGET;
            case ImageLayout::DEPTH_STENCIL_ATTACHMENT: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            case ImageLayout::TRANSFER_SRC: return D3D12_RESOURCE_STATE_COPY_SOURCE;
            case ImageLayout::TRANSFER_DST: return D3D12_RESOURCE_STATE_COPY_DEST;
            default: return D3D12_RESOURCE_STATE_COMMON;
            }
        }
        bool SupportedBoundary(const FrameGraphBuffer &buffer, D3D12_HEAP_TYPE heap)
        {
            auto supported = [heap](CopyBufferState state) {
                if (heap == D3D12_HEAP_TYPE_UPLOAD)
                    return state == CopyBufferState::HOST_WRITE || state == CopyBufferState::COPY_SOURCE;
                if (heap == D3D12_HEAP_TYPE_READBACK)
                    return state == CopyBufferState::HOST_READ || state == CopyBufferState::COPY_DESTINATION;
                return state == CopyBufferState::COMMON || state == CopyBufferState::RASTER_READ;
            };
            return supported(buffer.initial) && supported(buffer.final);
        }
        bool SupportedImageBoundary(ImageLayout layout)
        {
            return layout == ImageLayout::SHADER_READ_ONLY || layout == ImageLayout::GENERAL || layout == ImageLayout::UNDEFINED ||
                   layout == ImageLayout::COLOR_ATTACHMENT || layout == ImageLayout::TRANSFER_SRC || layout == ImageLayout::TRANSFER_DST ||
                   layout == ImageLayout::DEPTH_STENCIL_ATTACHMENT || layout == ImageLayout::PRESENT;
        }
        FrameError CompileError(HRESULT result)
        {
            return {FrameErrorCode::COMPILE_FAILED, {}, D3D12Error(result)};
        }
    } // namespace

    D3D12CompileResult D3D12FrameCompiler::Compile(std::shared_ptr<const FrozenFrameGraph> graph)
    {
        auto analyzed = AnalyzeFrameGraph(*graph);
        if (auto *error = std::get_if<FrameError>(&analyzed))
            return std::move(*error);
        const auto &analysis = std::get<FrameGraphAnalysis>(analyzed);
        if (analysis.usedBuffers.empty() && analysis.usedImages.empty())
            return FrameNoWork{};
        const auto &data         = graph->GetData();
        auto        plan         = std::make_unique<D3D12CompiledFrame>();
        const bool  depthResolve = std::any_of(analysis.livePasses.begin(), analysis.livePasses.end(), [&](auto index) {
            const auto *raster = std::get_if<FrameRasterPass>(&data.passes[index]);
            return raster && raster->depth && raster->depth->resolve;
        });
        if (depthResolve) {
            const auto                       &fn = device.GetFunctions();
            D3D12_FEATURE_DATA_D3D12_OPTIONS2 options{};
            auto                              status = fn.options2(device.GetNativeHandle(), &options);
            if (status == E_INVALIDARG)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Depth resolve capability query is unavailable"};
            if (FAILED(status))
                return CompileError(status);
            if (options.ProgrammableSamplePositionsTier < D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_1)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Native depth resolve is unavailable"};
            ComPtr<ID3D12CommandAllocator> allocator;
            status = fn.createAllocator(device.GetNativeHandle(), D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.GetAddressOf());
            if (FAILED(status))
                return CompileError(status);
            ComPtr<ID3D12GraphicsCommandList1> command;
            status = fn.createList1(device.GetNativeHandle(), D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), command.GetAddressOf());
            if (status == E_NOINTERFACE)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Command list depth resolve is unavailable"};
            if (FAILED(status))
                return CompileError(status);
            status = fn.closeList(command.Get());
            if (FAILED(status))
                return CompileError(status);
        }
        for (auto index : analysis.usedImages) {
            const auto &desc = data.images[index].descriptor;
            if (desc.samples == SampleCount::X1)
                continue;
            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality{FromPixelFormat(desc.format), static_cast<UINT>(desc.samples),
                                                                  D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE, 0};
            const auto                                    status = device.GetFunctions().multisampleQuality(device.GetNativeHandle(), &quality);
            if (FAILED(status))
                return CompileError(status);
            if (!quality.NumQualityLevels)
                return FrameError{FrameErrorCode::UNSUPPORTED, "MSAA sample count is unavailable"};
        }
        auto allocated = device.GetTransientPool().Acquire(data, analysis, lease.context.deviceEpoch);
        if (auto *error = std::get_if<FrameError>(&allocated))
            return std::move(*error);
        plan->resources              = std::move(std::get<FrameResourceBindings>(allocated));
        plan->livePasses             = analysis.livePasses;
        plan->encoder                = std::make_unique<D3D12FrameEncoder>();
        auto &encoder                = *plan->encoder;
        encoder.requiresDepthResolve = depthResolve;
        std::vector<D3D12Buffer *>                      buffers(data.buffers.size());
        std::vector<D3D12Image *>                       images(data.images.size());
        std::vector<D3D12_HEAP_TYPE>                    heaps(data.buffers.size());
        std::vector<D3D12_RESOURCE_STATES>              bufferStates(data.buffers.size());
        std::vector<std::vector<D3D12_RESOURCE_STATES>> imageStates(data.images.size());
        std::vector<std::vector<bool>>                  imageReady(data.images.size());
        for (auto i : analysis.usedBuffers) {
            const auto &input = data.buffers[i];
            if (!plan->resources.buffers[i] || plan->resources.buffers[i]->GetDevice() != &device)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto *buffer = static_cast<D3D12Buffer *>(plan->resources.buffers[i].Get());
            if (buffer->GetSize() != input.size)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            heaps[i] = FromMemoryType(buffer->GetDescriptor().memory);
            if (!SupportedBoundary(input, heaps[i]))
                return FrameError{FrameErrorCode::UNSUPPORTED};
            buffers[i]      = buffer;
            bufferStates[i] = BufferState(input.initial);
        }
        uint32_t targets = 0, depthTargets = 0;
        for (auto index : analysis.livePasses)
            if (const auto *raster = std::get_if<FrameRasterPass>(&data.passes[index])) {
                ++targets;
                if (raster->depth)
                    ++depthTargets;
            }
        D3D12_CPU_DESCRIPTOR_HANDLE firstTarget{};
        if (targets) {
            auto allocation = context.AllocateDescriptors(lease, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, targets);
            if (auto *error = std::get_if<FrameError>(&allocation))
                return std::move(*error);
            auto &range           = std::get<D3D12FrameDescriptors>(allocation);
            encoder.renderTargets = std::move(range.heap);
            firstTarget           = range.cpu;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE nextDepth{};
        if (depthTargets) {
            auto allocation = context.AllocateDescriptors(lease, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, depthTargets);
            if (auto *error = std::get_if<FrameError>(&allocation))
                return std::move(*error);
            auto &range          = std::get<D3D12FrameDescriptors>(allocation);
            encoder.depthTargets = std::move(range.heap);
            nextDepth            = range.cpu;
        }
        for (auto i : analysis.usedImages) {
            const auto &input = data.images[i];
            if (!plan->resources.images[i] || plan->resources.images[i]->GetDevice() != &device)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto       *image = static_cast<D3D12Image *>(plan->resources.images[i].Get());
            const auto &a     = input.descriptor;
            const auto &b     = image->GetDescriptor();
            if (!IsSameImageStorage(a, b))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            const auto native = image->GetNativeHandle()->GetDesc();
            if (native.MipLevels != b.mipLevels || native.DepthOrArraySize != b.arrayLayers)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Native image subresources do not match the declaration"};
            if (b.memory != MemoryType::GPU_ONLY || b.usage.TestBit(ImageUsageFlagBit::TRANSIENT))
                return FrameError{FrameErrorCode::UNSUPPORTED};
            auto validUsage = [&](ImageLayout state) {
                return state == ImageLayout::SHADER_READ_ONLY           ? b.usage.TestBit(ImageUsageFlagBit::SAMPLED)
                       : state == ImageLayout::COLOR_ATTACHMENT         ? b.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET)
                       : state == ImageLayout::DEPTH_STENCIL_ATTACHMENT ? b.usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL)
                       : state == ImageLayout::TRANSFER_SRC             ? b.usage.TestBit(ImageUsageFlagBit::TRANSFER_SRC)
                       : state == ImageLayout::TRANSFER_DST             ? b.usage.TestBit(ImageUsageFlagBit::TRANSFER_DST)
                                                                        : true;
            };
            const auto count = b.mipLevels * b.arrayLayers;
            for (uint32_t subresource = 0; subresource < count; ++subresource) {
                if (!SupportedImageBoundary(input.initial.At(subresource)) || !SupportedImageBoundary(input.final.At(subresource)))
                    return FrameError{FrameErrorCode::UNSUPPORTED};
                if (!validUsage(input.initial.At(subresource)) || !validUsage(input.final.At(subresource)))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            }
            images[i] = image;
            if (plan->resources.imageBacking[i] == i) {
                imageStates[i].reserve(count);
                for (uint32_t subresource = 0; subresource < count; ++subresource)
                    imageStates[i].push_back(ImageState(input.initial.At(subresource)));
                imageReady[i].assign(imageStates[i].size(), false);
                const auto initialized = ResolveInitializedImageContents(input);
                if (const auto *error = std::get_if<FrameError>(&initialized))
                    return *error;
                const auto &contents = std::get<ResolvedImageContents>(initialized);
                auto        span     = contents.ranges.begin();
                for (uint32_t subresource = 0; subresource < imageReady[i].size(); ++subresource) {
                    while (span != contents.ranges.end() && span->end <= contents.subresourceOffsets[subresource])
                        ++span;
                    // Any retained texel requires preserving this subresource's native metadata.
                    imageReady[i][subresource] = span != contents.ranges.end() && span->begin < contents.subresourceOffsets[subresource + 1];
                }
            }
        }
        std::vector<uint32_t>              bufferOwners(buffers.size(), UINT32_MAX);
        std::vector<std::vector<uint32_t>> imageOwners(images.size());
        for (auto i : analysis.usedImages)
            imageOwners[i].assign(imageStates[i].size(), UINT32_MAX);
        auto access = [&](uint32_t logical, uint32_t physical, ID3D12Resource *resource, auto &states, auto &owners, D3D12_RESOURCE_STATES target,
                          auto &out, uint32_t subresource) {
            // Reusing one native object still needs a dependency when both logical owners write in the same state.
            if (owners[physical] != UINT32_MAX && owners[physical] != logical && states[physical] == target &&
                target != D3D12_RESOURCE_STATE_COMMON) {
                out.push_back(Transition(resource, states[physical], D3D12_RESOURCE_STATE_COMMON, subresource));
                states[physical] = D3D12_RESOURCE_STATE_COMMON;
            }
            owners[physical] = logical;
            if (states[physical] != target) {
                out.push_back(Transition(resource, states[physical], target, subresource));
                states[physical] = target;
            }
        };
        auto bufferBarrier = [&](uint32_t i, D3D12_RESOURCE_STATES target, auto &out) {
            if (heaps[i] == D3D12_HEAP_TYPE_DEFAULT)
                access(i, plan->resources.bufferBacking[i], buffers[i]->GetNativeHandle(), bufferStates, bufferOwners, target, out,
                       D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        };
        auto imageBarrier = [&](uint32_t i, D3D12_RESOURCE_STATES target, auto &out, const ImageSubRange &range) {
            const auto physical = plan->resources.imageBacking[i];
            const auto mips     = data.images[i].descriptor.mipLevels;
            for (uint32_t layer = range.baseLayer; layer < range.baseLayer + range.layers; ++layer)
                for (uint32_t mip = range.baseLevel; mip < range.baseLevel + range.levels; ++mip) {
                    const auto subresource = layer * mips + mip;
                    access(i, subresource, images[i]->GetNativeHandle(), imageStates[physical], imageOwners[physical], target, out, subresource);
                }
        };
        auto imageWriteBarrier = [&](uint32_t i, D3D12_RESOURCE_STATES target, auto &out, const ImageSubRange &range,
                                     bool needsInitialization) -> std::optional<D3D12_RESOURCE_BARRIER> {
            const auto physical        = plan->resources.imageBacking[i];
            const auto subresource     = range.baseLayer * data.images[i].descriptor.mipLevels + range.baseLevel;
            const auto usage           = data.images[i].descriptor.usage;
            const bool depth           = usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL);
            const auto initializeState = depth ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_RENDER_TARGET;
            const bool discard =
                needsInitialization && !imageReady[physical][subresource] && (depth || usage.TestBit(ImageUsageFlagBit::RENDER_TARGET));
            imageBarrier(i, discard ? initializeState : target, out, range);
            imageReady[physical][subresource] = true;
            if (!discard)
                return {};
            // Discard initializes native metadata only; graph content is still defined by the actual write.
            imageStates[physical][subresource] = target;
            return Transition(images[i]->GetNativeHandle(), initializeState, target, subresource);
        };
        auto validBufferUsage = [&](uint32_t i, bool source) {
            return buffers[i]->GetDescriptor().usage.TestBit(source ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST) &&
                   heaps[i] != (source ? D3D12_HEAP_TYPE_READBACK : D3D12_HEAP_TYPE_UPLOAD);
        };
        bool scratchUsed = false;
        for (auto index : analysis.livePasses) {
            D3D12EncodeTask task;
            task.logicalPass = index;
            if (const auto *copy = std::get_if<BufferCopyPass>(&data.passes[index])) {
                const auto src = copy->source.index, dst = copy->destination.index;
                if (!validBufferUsage(src, true) || !validBufferUsage(dst, false))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                bufferBarrier(src, D3D12_RESOURCE_STATE_COPY_SOURCE, task.before);
                bufferBarrier(dst, D3D12_RESOURCE_STATE_COPY_DEST, task.before);
                task.operation = D3D12BufferCopyStep{buffers[src]->GetNativeHandle(), buffers[dst]->GetNativeHandle(), copy->sourceOffset,
                                                     copy->destinationOffset, copy->size};
                encoder.tasks.push_back(std::move(task));
            } else if (const auto *copy = std::get_if<BufferImageCopyPass>(&data.passes[index])) {
                const auto bi = copy->buffer.index, im = copy->image.index;
                const bool upload = copy->direction == ImageCopyDirection::BUFFER_TO_IMAGE;
                if (!validBufferUsage(bi, upload) ||
                    !images[im]->GetDescriptor().usage.TestBit(upload ? ImageUsageFlagBit::TRANSFER_DST : ImageUsageFlagBit::TRANSFER_SRC))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                bufferBarrier(bi, upload ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_COPY_DEST, task.before);
                D3D12ImageCopyStep step;
                step.buffer                   = buffers[bi]->GetNativeHandle();
                step.image                    = images[im]->GetNativeHandle();
                step.bufferOffset             = copy->region.bufferOffset;
                step.subresource              = copy->region.subRange.level + copy->region.subRange.baseLayer * data.images[im].descriptor.mipLevels;
                step.upload                   = upload;
                const auto         &imageDesc = data.images[im].descriptor;
                const bool          partial = copy->region.imageExtent.width != std::max(1u, imageDesc.extent.width >> copy->region.subRange.level) ||
                                              copy->region.imageExtent.height != std::max(1u, imageDesc.extent.height >> copy->region.subRange.level);
                const ImageSubRange range{copy->region.subRange.level, 1, copy->region.subRange.baseLayer, 1};
                if (upload)
                    task.initializeImage = imageWriteBarrier(im, D3D12_RESOURCE_STATE_COPY_DEST, task.before, range, partial);
                else
                    imageBarrier(im, D3D12_RESOURCE_STATE_COPY_SOURCE, task.before, range);
                const auto &region  = copy->region;
                step.bufferRowPitch = uint64_t(region.bufferRowLength ? region.bufferRowLength : region.imageExtent.width) * 4;
                step.imageBox       = {static_cast<UINT>(region.imageOffset.x),
                                       static_cast<UINT>(region.imageOffset.y),
                                       0,
                                       static_cast<UINT>(region.imageOffset.x) + region.imageExtent.width,
                                       static_cast<UINT>(region.imageOffset.y) + region.imageExtent.height,
                                       1};
                auto desc           = step.image->GetDesc();
                desc.Width          = region.imageExtent.width;
                desc.Height         = region.imageExtent.height;
                desc.MipLevels = desc.DepthOrArraySize = 1;
                uint64_t bytes                         = 0;
                device.GetNativeHandle()->GetCopyableFootprints(&desc, 0, 1, 0, &step.footprint, &step.rows, &step.rowBytes, &bytes);
                if (!bytes || bytes == UINT64_MAX)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                auto allocation = context.AllocateCopyScratch(lease, bytes);
                if (auto *error = std::get_if<FrameError>(&allocation))
                    return std::move(*error);
                auto &slice           = std::get<FrameBufferSlice>(allocation);
                step.scratch          = std::move(slice.buffer);
                step.footprint.Offset = slice.offset;
                task.before.push_back(Transition(static_cast<D3D12Buffer *>(step.scratch.Get())->GetNativeHandle(),
                                                 scratchUsed ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_COMMON,
                                                 D3D12_RESOURCE_STATE_COPY_DEST));
                scratchUsed    = true;
                task.operation = std::move(step);
                encoder.tasks.push_back(std::move(task));
            } else {
                const auto &pass = std::get<FrameRasterPass>(data.passes[index]);
                if (pass.colorLocalRead)
                    return FrameError{FrameErrorCode::UNSUPPORTED, "Shader color local read is not implemented"};
                auto *image = images[pass.color.index];
                if (!image->GetDescriptor().usage.TestBit(ImageUsageFlagBit::RENDER_TARGET))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                const auto view = std::get<ResolvedRasterAttachmentView>(ResolveRasterAttachmentView(pass.colorView, image->GetDescriptor()));
                imageBarrier(pass.color.index, D3D12_RESOURCE_STATE_RENDER_TARGET, task.before, view.range);
                if (pass.load != LoadOp::DONT_CARE)
                    imageReady[plan->resources.imageBacking[pass.color.index]][view.subresource] = true;
                D3D12RasterScope scope;
                scope.target = firstTarget;
                scope.target.ptr +=
                    encoder.scopes.size() * device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
                D3D12_RENDER_TARGET_VIEW_DESC target{};
                target.Format = FromPixelFormat(data.images[pass.color.index].descriptor.format);
                if (image->GetDescriptor().samples != SampleCount::X1) {
                    if (image->GetDescriptor().arrayLayers > 1) {
                        target.ViewDimension    = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        target.Texture2DMSArray = {pass.colorView.layer, 1};
                    } else {
                        target.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                    }
                } else if (image->GetDescriptor().arrayLayers > 1) {
                    target.ViewDimension  = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                    target.Texture2DArray = {pass.colorView.mip, pass.colorView.layer, 1, 0};
                } else {
                    target.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
                    target.Texture2D.MipSlice = pass.colorView.mip;
                }
                device.GetNativeHandle()->CreateRenderTargetView(image->GetNativeHandle(), &target, scope.target);
                scope.viewport = {0.f, 0.f, static_cast<float>(view.extent.width), static_cast<float>(view.extent.height), 0.f, 1.f};
                scope.load     = pass.load;
                scope.clear    = pass.clear;
                if (pass.depth) {
                    auto *depth = images[pass.depth->image.index];
                    if (!depth->GetDescriptor().usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL))
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                    const auto depthView =
                        std::get<ResolvedRasterAttachmentView>(ResolveRasterAttachmentView(pass.depth->view, depth->GetDescriptor()));
                    imageBarrier(pass.depth->image.index, D3D12_RESOURCE_STATE_DEPTH_WRITE, task.before, depthView.range);
                    if (pass.depth->load != LoadOp::DONT_CARE)
                        imageReady[plan->resources.imageBacking[pass.depth->image.index]][depthView.subresource] = true;
                    scope.depth = nextDepth;
                    nextDepth.ptr += device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
                    D3D12_DEPTH_STENCIL_VIEW_DESC depthTarget{};
                    depthTarget.Format = DXGI_FORMAT_D32_FLOAT;
                    if (depth->GetDescriptor().samples != SampleCount::X1) {
                        if (depth->GetDescriptor().arrayLayers > 1) {
                            depthTarget.ViewDimension    = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                            depthTarget.Texture2DMSArray = {pass.depth->view.layer, 1};
                        } else {
                            depthTarget.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                        }
                    } else if (depth->GetDescriptor().arrayLayers > 1) {
                        depthTarget.ViewDimension  = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                        depthTarget.Texture2DArray = {pass.depth->view.mip, pass.depth->view.layer, 1};
                    } else {
                        depthTarget.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
                        depthTarget.Texture2D.MipSlice = pass.depth->view.mip;
                    }
                    device.GetNativeHandle()->CreateDepthStencilView(depth->GetNativeHandle(), &depthTarget, scope.depth);
                    scope.depthLoad  = pass.depth->load;
                    scope.depthClear = pass.depth->clear;
                }
                std::vector<bool> rasterBuffers(buffers.size());
                auto              read = [&](GraphBufferHandle handle, BufferUsageFlagBit usage) {
                    if (heaps[handle.index] == D3D12_HEAP_TYPE_READBACK || !buffers[handle.index]->GetDescriptor().usage.TestBit(usage))
                        return false;
                    rasterBuffers[handle.index] = true;
                    return true;
                };
                auto less = [](const RasterDraw *a, const RasterDraw *b) { return RasterPipelineLess(*a, *b); };
                std::map<const RasterDraw *, CounterPtr<D3D12GraphicsPipeline>, decltype(less)> pipelines(less);
                for (const auto &draw : pass.draws) {
                    if (!draw.shader || draw.shader->GetDevice() != &device)
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                    auto *shader = static_cast<D3D12Shader *>(draw.shader.Get());
                    if (!shader->IsGraphics())
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                    const auto &reflection = shader->GetReflection();
                    const auto &blend      = draw.blend;
                    if (auto error = ValidateRasterBindings(draw, reflection))
                        return std::move(*error);
                    if (blend.srcColor >= BlendFactor::SRC1_COLOR || blend.dstColor >= BlendFactor::SRC1_COLOR ||
                        blend.srcAlpha >= BlendFactor::SRC1_COLOR || blend.dstAlpha >= BlendFactor::SRC1_COLOR)
                        return FrameError{FrameErrorCode::UNSUPPORTED};
                    auto                         state = RasterPipelineState(draw);
                    GraphicsPipeline::Descriptor desc;
                    desc.state                    = &state;
                    desc.shader                   = shader;
                    desc.format.sampleCount       = image->GetDescriptor().samples;
                    state.multiSample.sampleCount = desc.format.sampleCount;
                    desc.format.numColors         = 1;
                    desc.format.colors[0]         = image->GetPixelFormat();
                    desc.format.depthStencil      = pass.depth ? PixelFormat::D32 : PixelFormat::UNDEFINED;
                    auto &pipeline                = pipelines[&draw];
                    if (!pipeline) {
                        auto cached =
                            device.GetFramePipelineCache().Acquire(FramePipelineKey{draw, desc.format, lease.context.deviceEpoch},
                                                                   [&]() -> std::variant<FrameError, CounterPtr<GraphicsPipeline>> {
                                                                       CounterPtr<D3D12GraphicsPipeline> created(new D3D12GraphicsPipeline(device));
                                                                       if (!created->Init(desc))
                                                                           return CompileError(created->GetCreateResult());
                                                                       return CounterPtr<GraphicsPipeline>(created.Get());
                                                                   });
                        if (auto *error = std::get_if<FrameError>(&cached))
                            return std::move(*error);
                        pipeline.Reset(static_cast<D3D12GraphicsPipeline *>(std::get<CounterPtr<GraphicsPipeline>>(cached).Get()));
                    }
                    const auto &rect = draw.scissor;
                    D3D12_RECT  scissor{rect.offset.x, rect.offset.y, rect.offset.x + static_cast<LONG>(rect.extent.width),
                                        rect.offset.y + static_cast<LONG>(rect.extent.height)};
                    if (!rect.extent.width)
                        scissor = {0, 0, static_cast<LONG>(scope.viewport.Width), static_cast<LONG>(scope.viewport.Height)};
                    scope.draws.push_back({pipeline, draw.command, scissor});
                    auto &native = scope.draws.back();
                    for (const auto &stream : draw.vertices) {
                        if (!read(stream.range.buffer, BufferUsageFlagBit::VERTEX))
                            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid vertex buffer usage/heap"};
                        native.vertices.push_back(
                            {buffers[stream.range.buffer.index]->GetNativeHandle()->GetGPUVirtualAddress() + stream.range.offset,
                             static_cast<UINT>(stream.range.size), stream.stride});
                    }
                    if (draw.indexed) {
                        if (!read(draw.indexed->range.buffer, BufferUsageFlagBit::INDEX))
                            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid index buffer usage/heap"};
                        native.index   = {buffers[draw.indexed->range.buffer.index]->GetNativeHandle()->GetGPUVirtualAddress() +
                                              draw.indexed->range.offset,
                                          static_cast<UINT>(draw.indexed->range.size),
                                          draw.indexed->type == IndexType::U16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT};
                        native.indexed = draw.indexed->command;
                    }
                    for (uint32_t set = 0; set < 3; ++set) {
                        const auto count =
                            static_cast<uint32_t>(std::count_if(reflection.resources.begin(), reflection.resources.end(), [&](const auto &r) {
                                return r.set == set && (r.type == ShaderResourceType::UNIFORM_BUFFER || r.type == ShaderResourceType::SAMPLED_IMAGE);
                            }));
                        D3D12_CPU_DESCRIPTOR_HANDLE descriptor{};
                        if (count) {
                            auto allocated = context.AllocateDescriptors(lease, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, count);
                            if (auto *error = std::get_if<FrameError>(&allocated))
                                return std::move(*error);
                            auto &range       = std::get<D3D12FrameDescriptors>(allocated);
                            encoder.resources = range.heap;
                            descriptor        = range.cpu;
                            native.tables.push_back({shader->GetRootSignature()->GetCbvSrvUavRootParam(set), range.gpu});
                        }
                        const auto samplerCount = static_cast<uint32_t>(
                            std::count_if(draw.samplers.begin(), draw.samplers.end(), [&](const auto &binding) { return binding.set == set; }));
                        D3D12_CPU_DESCRIPTOR_HANDLE samplerDescriptor{};
                        if (samplerCount) {
                            auto allocated = context.AllocateDescriptors(lease, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, samplerCount);
                            if (auto *error = std::get_if<FrameError>(&allocated))
                                return std::move(*error);
                            auto &range       = std::get<D3D12FrameDescriptors>(allocated);
                            encoder.samplers  = range.heap;
                            samplerDescriptor = range.cpu;
                            native.tables.push_back({shader->GetRootSignature()->GetSamplerRootParam(set), range.gpu});
                        }
                        for (const auto &resource : reflection.resources) {
                            if (resource.set != set)
                                continue;
                            auto matches = [&](const auto &binding) { return binding.set == set && binding.binding == resource.binding; };
                            if (resource.type == ShaderResourceType::SAMPLED_IMAGE) {
                                const auto binding = std::find_if(draw.images.begin(), draw.images.end(), matches);
                                auto      *sampled = images[binding->image.index];
                                if (!sampled->GetDescriptor().usage.TestBit(ImageUsageFlagBit::SAMPLED))
                                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing sampled image usage"};
                                D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
                                srv.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
                                auto resolvedView = ResolveRasterImageView(binding->view, sampled->GetDescriptor());
                                if (const auto *error = std::get_if<FrameError>(&resolvedView))
                                    return *error;
                                const auto &view            = std::get<RasterImageView>(resolvedView);
                                srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                                if (view.type == ImageViewType::VIEW_2D_ARRAY) {
                                    srv.ViewDimension                  = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                                    srv.Texture2DArray.MostDetailedMip = view.baseMip;
                                    srv.Texture2DArray.MipLevels       = view.mipCount;
                                    srv.Texture2DArray.FirstArraySlice = view.baseLayer;
                                    srv.Texture2DArray.ArraySize       = view.layerCount;
                                } else {
                                    srv.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
                                    srv.Texture2D.MostDetailedMip = view.baseMip;
                                    srv.Texture2D.MipLevels       = view.mipCount;
                                }
                                device.GetNativeHandle()->CreateShaderResourceView(sampled->GetNativeHandle(), &srv, descriptor);
                                descriptor.ptr += device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                                imageBarrier(binding->image.index, ImageState(ImageLayout::SHADER_READ_ONLY), task.before,
                                             {view.baseMip, view.mipCount, view.baseLayer, view.layerCount});
                                continue;
                            }
                            if (resource.type == ShaderResourceType::SAMPLER) {
                                const auto binding = std::find_if(draw.samplers.begin(), draw.samplers.end(), matches);
                                if (binding->sampler->GetDevice() != &device)
                                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Sampler belongs to another or unknown device"};
                                const auto &desc = static_cast<const D3D12Sampler *>(binding->sampler.Get())->GetNativeDesc();
                                device.GetNativeHandle()->CreateSampler(&desc, samplerDescriptor);
                                samplerDescriptor.ptr +=
                                    device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                                continue;
                            }
                            const auto binding = std::find_if(draw.uniforms.begin(), draw.uniforms.end(),
                                                              [&](const auto &b) { return b.set == set && b.binding == resource.binding; });
                            auto      *buffer  = buffers[binding->range.buffer.index];
                            const auto offset  = binding->range.offset + binding->dynamicOffset;
                            const auto padded  = (binding->range.size + 255) & ~uint64_t{255};
                            if (!read(binding->range.buffer, BufferUsageFlagBit::UNIFORM) || binding->range.offset % 256 ||
                                binding->dynamicOffset % 256 || padded > buffer->GetSize() - offset)
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid DX12 UBO alignment/range/heap"};
                            const auto address = buffer->GetNativeHandle()->GetGPUVirtualAddress() + offset;
                            if (resource.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC)
                                native.uniforms.push_back({shader->GetRootSignature()->GetDynamicRootParam(set, resource.binding), address});
                            else {
                                const D3D12_CONSTANT_BUFFER_VIEW_DESC cbv{address, static_cast<UINT>(padded)};
                                device.GetNativeHandle()->CreateConstantBufferView(&cbv, descriptor);
                                descriptor.ptr += device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                            }
                        }
                    }
                }
                for (uint32_t i = 0; i < rasterBuffers.size(); ++i)
                    if (rasterBuffers[i])
                        bufferBarrier(i, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_INDEX_BUFFER, task.before);
                const auto scopeIndex = static_cast<uint32_t>(encoder.scopes.size());
                const auto count      = static_cast<uint32_t>(scope.draws.size());
                encoder.scopes.push_back(std::move(scope));
                for (uint32_t first = 0;;) {
                    task.operation = D3D12RasterFragment{scopeIndex, first, std::min(32u, count - first)};
                    encoder.tasks.push_back(std::move(task));
                    if (count - first <= 32)
                        break;
                    first += 32;
                    task             = {};
                    task.logicalPass = index;
                }
                if (pass.colorResolve) {
                    const auto &resolve    = *pass.colorResolve;
                    const auto  targetView = std::get<ResolvedRasterAttachmentView>(
                        ResolveRasterAttachmentView(resolve.view, images[resolve.image.index]->GetDescriptor()));
                    D3D12EncodeTask resolveTask;
                    resolveTask.logicalPass = index;
                    imageBarrier(pass.color.index, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, resolveTask.before, view.range);
                    resolveTask.initializeImage =
                        imageWriteBarrier(resolve.image.index, D3D12_RESOURCE_STATE_RESOLVE_DEST, resolveTask.before, targetView.range, true);
                    resolveTask.operation = D3D12ColorResolveStep{image->GetNativeHandle(), images[resolve.image.index]->GetNativeHandle(),
                                                                  view.subresource, targetView.subresource};
                    encoder.tasks.push_back(std::move(resolveTask));
                }
                if (pass.depth && pass.depth->resolve) {
                    const auto &depth   = *pass.depth;
                    const auto &resolve = *depth.resolve;
                    const auto  sourceView =
                        std::get<ResolvedRasterAttachmentView>(ResolveRasterAttachmentView(depth.view, images[depth.image.index]->GetDescriptor()));
                    const auto targetView = std::get<ResolvedRasterAttachmentView>(
                        ResolveRasterAttachmentView(resolve.view, images[resolve.image.index]->GetDescriptor()));
                    D3D12EncodeTask resolveTask;
                    resolveTask.logicalPass = index;
                    imageBarrier(depth.image.index, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, resolveTask.before, sourceView.range);
                    resolveTask.initializeImage =
                        imageWriteBarrier(resolve.image.index, D3D12_RESOURCE_STATE_RESOLVE_DEST, resolveTask.before, targetView.range, true);
                    resolveTask.operation =
                        D3D12DepthResolveStep{{images[depth.image.index]->GetNativeHandle(), images[resolve.image.index]->GetNativeHandle(),
                                               sourceView.subresource, targetView.subresource},
                                              resolve.mode == DepthResolveMode::MINIMUM ? D3D12_RESOLVE_MODE_MIN : D3D12_RESOLVE_MODE_MAX};
                    encoder.tasks.push_back(std::move(resolveTask));
                }
            }
        }
        std::vector<D3D12_RESOURCE_BARRIER> final;
        for (auto i : analysis.usedBuffers)
            if (plan->resources.bufferBacking[i] == i)
                bufferBarrier(i, BufferState(data.buffers[i].final), final);
        for (auto i : analysis.usedImages)
            if (plan->resources.imageBacking[i] == i) {
                const auto &desc = data.images[i].descriptor;
                for (uint32_t layer = 0; layer < desc.arrayLayers; ++layer)
                    for (uint32_t mip = 0; mip < desc.mipLevels; ++mip)
                        imageBarrier(i, ImageState(data.images[i].final.At(layer * desc.mipLevels + mip)), final, {mip, 1, layer, 1});
            }
        if (encoder.tasks.empty()) {
            if (final.empty())
                return FrameNoWork{};
            encoder.tasks.emplace_back();
            encoder.tasks.back().logicalPass = UINT32_MAX;
        }
        encoder.tasks.back().after = std::move(final);
        plan->graph                = std::move(graph);
        return plan;
    }

    void D3D12FrameEncoder::Encode(const D3D12Functions       &fn,
                                   ID3D12GraphicsCommandList  *command,
                                   uint32_t                    index,
                                   ID3D12GraphicsCommandList1 *extended) const
    {
        const auto &task = tasks[index];
        if (!task.before.empty())
            command->ResourceBarrier(static_cast<UINT>(task.before.size()), task.before.data());
        if (task.initializeImage) {
            const auto                &transition = task.initializeImage->Transition;
            const D3D12_DISCARD_REGION region{0, nullptr, transition.Subresource, 1};
            command->DiscardResource(transition.pResource, &region);
            command->ResourceBarrier(1, &*task.initializeImage);
        }
        if (const auto *copy = std::get_if<D3D12BufferCopyStep>(&task.operation))
            command->CopyBufferRegion(copy->destination, copy->destinationOffset, copy->source, copy->sourceOffset, copy->size);
        else if (const auto *copy = std::get_if<D3D12ImageCopyStep>(&task.operation)) {
            auto                       *scratch = static_cast<D3D12Buffer *>(copy->scratch.Get())->GetNativeHandle();
            D3D12_TEXTURE_COPY_LOCATION texture{}, buffer{};
            texture.pResource        = copy->image;
            texture.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            texture.SubresourceIndex = copy->subresource;
            buffer.pResource         = scratch;
            buffer.Type              = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            buffer.PlacedFootprint   = copy->footprint;
            if (copy->upload)
                for (uint32_t row = 0; row < copy->rows; ++row)
                    command->CopyBufferRegion(scratch, copy->footprint.Offset + uint64_t{row} * copy->footprint.Footprint.RowPitch, copy->buffer,
                                              copy->bufferOffset + row * copy->bufferRowPitch, copy->rowBytes);
            else
                command->CopyTextureRegion(&buffer, 0, 0, 0, &texture, &copy->imageBox);
            const auto barrier = Transition(scratch, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
            command->ResourceBarrier(1, &barrier);
            if (copy->upload)
                command->CopyTextureRegion(&texture, copy->imageBox.left, copy->imageBox.top, 0, &buffer, nullptr);
            else
                for (uint32_t row = 0; row < copy->rows; ++row)
                    command->CopyBufferRegion(copy->buffer, copy->bufferOffset + row * copy->bufferRowPitch, scratch,
                                              copy->footprint.Offset + uint64_t{row} * copy->footprint.Footprint.RowPitch, copy->rowBytes);
        } else if (const auto *resolve = std::get_if<D3D12ColorResolveStep>(&task.operation)) {
            command->ResolveSubresource(resolve->destination, resolve->destinationSubresource, resolve->source, resolve->sourceSubresource,
                                        DXGI_FORMAT_R8G8B8A8_UNORM);
        } else if (const auto *resolve = std::get_if<D3D12DepthResolveStep>(&task.operation)) {
            extended->ResolveSubresourceRegion(resolve->destination, resolve->destinationSubresource, 0, 0, resolve->source,
                                               resolve->sourceSubresource, nullptr, DXGI_FORMAT_D32_FLOAT, resolve->mode);
        } else if (const auto *raster = std::get_if<D3D12RasterFragment>(&task.operation)) {
            const auto &fragment = *raster;
            const auto &scope    = scopes[fragment.scope];
            command->OMSetRenderTargets(1, &scope.target, FALSE, scope.depth.ptr ? &scope.depth : nullptr);
            if (!fragment.firstDraw && scope.load == LoadOp::CLEAR)
                command->ClearRenderTargetView(scope.target, scope.clear.color.float32, 0, nullptr);
            if (!fragment.firstDraw && scope.depth.ptr && scope.depthLoad == LoadOp::CLEAR)
                command->ClearDepthStencilView(scope.depth, D3D12_CLEAR_FLAG_DEPTH, scope.depthClear, 0, 0, nullptr);
            ID3D12DescriptorHeap *heaps[2]{};
            uint32_t              heapCount = 0;
            if (resources)
                heaps[heapCount++] = resources.Get();
            if (samplers)
                heaps[heapCount++] = samplers.Get();
            if (heapCount)
                command->SetDescriptorHeaps(heapCount, heaps);
            command->RSSetViewports(1, &scope.viewport);
            for (uint32_t i = fragment.firstDraw; i < fragment.firstDraw + fragment.drawCount; ++i) {
                const auto &draw = scope.draws[i];
                command->SetPipelineState(draw.pipeline->GetNativeHandle());
                command->SetGraphicsRootSignature(draw.pipeline->GetRootSignature()->GetNativeHandle());
                command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                command->RSSetScissorRects(1, &draw.scissor);
                for (const auto &table : draw.tables)
                    command->SetGraphicsRootDescriptorTable(table.root, table.descriptor);
                for (const auto &binding : draw.uniforms)
                    command->SetGraphicsRootConstantBufferView(binding.root, binding.address);
                if (!draw.vertices.empty())
                    command->IASetVertexBuffers(0, static_cast<UINT>(draw.vertices.size()), draw.vertices.data());
                if (draw.index.BufferLocation) {
                    command->IASetIndexBuffer(&draw.index);
                    command->DrawIndexedInstanced(draw.indexed.indexCount, draw.indexed.instanceCount, draw.indexed.firstIndex,
                                                  draw.indexed.vertexOffset, draw.indexed.firstInstance);
                } else
                    fn.draw(command, draw.command.vertexCount, draw.command.instanceCount, draw.command.firstVertex, draw.command.firstInstance);
            }
        }
        if (!task.after.empty())
            command->ResourceBarrier(static_cast<UINT>(task.after.size()), task.after.data());
    }
} // namespace sky::aurora
