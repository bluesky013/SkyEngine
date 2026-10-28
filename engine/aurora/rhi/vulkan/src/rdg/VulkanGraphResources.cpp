#include "VulkanGraphResources.h"

#include <aurora/rdg/FrameBindings.h>

#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanInstance.h>
#include <VulkanSampler.h>
#include <VulkanShader.h>
#include <algorithm>
#include <map>
#include <rdg/VulkanDeviceFrameContext.h>

namespace sky::aurora {
    namespace {
        VulkanResourceAccess BufferAccess(CopyBufferState state)
        {
            switch (state) {
            case CopyBufferState::HOST_WRITE: return {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT};
            case CopyBufferState::HOST_READ: return {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
            case CopyBufferState::COPY_SOURCE: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
            case CopyBufferState::COPY_DESTINATION: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
            case CopyBufferState::RASTER_READ:
                return {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT |
                            VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
            case CopyBufferState::SHADER_READ:
                return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
            case CopyBufferState::UNORDERED_ACCESS:
                // Custom may use storage from graphics; the state must cover both sides of that pass.
                return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            default: return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
            }
        }
        VulkanResourceAccess ImageAccess(ImageLayout state)
        {
            switch (state) {
            case ImageLayout::SHADER_READ_ONLY:
                return {VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                        VK_ACCESS_2_SHADER_SAMPLED_READ_BIT};
            case ImageLayout::GENERAL:
                return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            case ImageLayout::UNDEFINED: return {VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE};
            case ImageLayout::COLOR_ATTACHMENT:
                return {VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
            case ImageLayout::TRANSFER_SRC: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
            case ImageLayout::TRANSFER_DST: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
            case ImageLayout::DEPTH_STENCIL_ATTACHMENT:
                // The same layout can contain fixed-function depth resolve writes at COLOR_ATTACHMENT_OUTPUT.
                return {VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT |
                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT};
            default: return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
            }
        }
        bool ImageBoundary(ImageLayout state)
        {
            return state == ImageLayout::SHADER_READ_ONLY || state == ImageLayout::UNDEFINED || state == ImageLayout::GENERAL ||
                   state == ImageLayout::COLOR_ATTACHMENT || state == ImageLayout::TRANSFER_SRC || state == ImageLayout::TRANSFER_DST ||
                   state == ImageLayout::DEPTH_STENCIL_ATTACHMENT || state == ImageLayout::PRESENT;
        }
    } // namespace
    FrameError VulkanFrameError(VkResult error, FrameErrorCode phase) noexcept
    {
        const auto code = error == VK_ERROR_DEVICE_LOST                                                    ? GpuErrorCode::DEVICE_LOST
                          : error == VK_ERROR_OUT_OF_HOST_MEMORY || error == VK_ERROR_OUT_OF_DEVICE_MEMORY ? GpuErrorCode::OUT_OF_MEMORY
                                                                                                           : GpuErrorCode::NATIVE_ERROR;
        return {phase, {}, {code, error}};
    }
    void VulkanBarriers::Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command) const noexcept
    {
        if (buffers.empty() && images.empty())
            return;
        VkDependencyInfo info{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        info.bufferMemoryBarrierCount = static_cast<uint32_t>(buffers.size());
        info.pBufferMemoryBarriers    = buffers.data();
        info.imageMemoryBarrierCount  = static_cast<uint32_t>(images.size());
        info.pImageMemoryBarriers     = images.data();
        fn.vkCmdPipelineBarrier2(command, &info);
    }
    void VulkanTransferStep::Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command) const noexcept
    {
        before.Encode(fn, command);
        if (!image)
            fn.vkCmdCopyBuffer(command, source, destination, 1, &bufferRegion);
        else if (source)
            fn.vkCmdCopyBufferToImage(command, source, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageRegion);
        else
            fn.vkCmdCopyImageToBuffer(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destination, 1, &imageRegion);
    }
    std::optional<FrameError> VulkanGraphResources::Resolve(const FrameGraphAnalysis &analysis)
    {
        for (auto index : analysis.livePasses) {
            const auto *raster = std::get_if<FrameRasterPass>(&data.passes[index]);
            if (!raster || !raster->depth || !raster->depth->resolve)
                continue;
            VkPhysicalDeviceDepthStencilResolveProperties resolve{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DEPTH_STENCIL_RESOLVE_PROPERTIES};
            VkPhysicalDeviceProperties2                   properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &resolve};
            device.GetVulkanInstance().GetInstanceFn().vkGetPhysicalDeviceProperties2(device.GetGpuHandle(), &properties);
            const auto mode = raster->depth->resolve->mode == DepthResolveMode::MINIMUM ? VK_RESOLVE_MODE_MIN_BIT : VK_RESOLVE_MODE_MAX_BIT;
            if (!(resolve.supportedDepthResolveModes & mode))
                return FrameError{FrameErrorCode::UNSUPPORTED, "Depth resolve mode is unavailable"};
        }
        for (auto index : analysis.usedImages) {
            const auto &desc = data.images[index].descriptor;
            if (desc.samples == SampleCount::X1 && desc.format != PixelFormat::RGBA16_SFLOAT &&
                !(desc.format == PixelFormat::D32 && desc.usage.TestBit(ImageUsageFlagBit::SAMPLED)))
                continue;
            VkImageFormatProperties properties{};
            const auto              status = device.GetVulkanInstance().GetInstanceFn().vkGetPhysicalDeviceImageFormatProperties(
                device.GetGpuHandle(), FromPixelFormat(desc.format), VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, FromImageUsageFlags(desc.usage), 0,
                &properties);
            if (status == VK_ERROR_FORMAT_NOT_SUPPORTED)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Image format/usage combination is unavailable"};
            if (status != VK_SUCCESS)
                return VulkanFrameError(status, FrameErrorCode::COMPILE_FAILED);
            if (!(properties.sampleCounts & FromSampleCount(desc.samples)) || desc.extent.width > properties.maxExtent.width ||
                desc.extent.height > properties.maxExtent.height || desc.arrayLayers > properties.maxArrayLayers ||
                desc.mipLevels > properties.maxMipLevels)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Image exceeds device format limits"};
        }
        auto allocated = device.GetTransientPool().Acquire(data, analysis, epoch);
        if (auto *error = std::get_if<FrameError>(&allocated))
            return std::move(*error);
        bindings = std::move(std::get<FrameResourceBindings>(allocated));
        buffers.resize(data.buffers.size());
        bufferStates.resize(data.buffers.size());
        images.resize(data.images.size());
        imageStates.resize(data.images.size());
        for (auto i : analysis.usedBuffers) {
            const auto &input = data.buffers[i];
            if (!bindings.buffers[i] || bindings.buffers[i]->GetDevice() != &device || bindings.buffers[i]->GetSize() != input.size)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto      *buffer = static_cast<VulkanBuffer *>(bindings.buffers[i].Get());
            const bool host   = input.initial == CopyBufferState::HOST_READ || input.initial == CopyBufferState::HOST_WRITE ||
                              input.final == CopyBufferState::HOST_READ || input.final == CopyBufferState::HOST_WRITE;
            if (host && !buffer->IsHostCoherent())
                return FrameError{FrameErrorCode::UNSUPPORTED};
            buffers[i]      = buffer;
            bufferStates[i] = BufferAccess(input.initial);
        }
        for (auto i : analysis.usedImages) {
            const auto &input = data.images[i];
            if (!bindings.images[i] || bindings.images[i]->GetDevice() != &device)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto *image = static_cast<VulkanImage *>(bindings.images[i].Get());
            if (!image->IsOwned() && !input.image.IsBorrowed())
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            const auto &actual = image->GetDescriptor();
            const auto &desc   = input.descriptor;
            if (!IsSameImageStorage(desc, actual))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            if (desc.usage.TestBit(ImageUsageFlagBit::TRANSIENT))
                return FrameError{FrameErrorCode::UNSUPPORTED};
            auto validUsage = [&](ImageLayout state) {
                return state == ImageLayout::SHADER_READ_ONLY           ? desc.usage.TestBit(ImageUsageFlagBit::SAMPLED)
                       : state == ImageLayout::COLOR_ATTACHMENT         ? desc.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET)
                       : state == ImageLayout::DEPTH_STENCIL_ATTACHMENT ? desc.usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL)
                       : state == ImageLayout::TRANSFER_SRC             ? desc.usage.TestBit(ImageUsageFlagBit::TRANSFER_SRC)
                       : state == ImageLayout::TRANSFER_DST             ? desc.usage.TestBit(ImageUsageFlagBit::TRANSFER_DST)
                                                                        : true;
            };
            const auto count = desc.mipLevels * desc.arrayLayers;
            for (uint32_t subresource = 0; subresource < count; ++subresource) {
                if (!ImageBoundary(input.initial.At(subresource)) || !ImageBoundary(input.final.At(subresource)))
                    return FrameError{FrameErrorCode::UNSUPPORTED};
                if (!validUsage(input.initial.At(subresource)) || !validUsage(input.final.At(subresource)))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            }
            images[i] = image;
            if (bindings.imageBacking[i] == i) {
                imageStates[i].reserve(count);
                for (uint32_t subresource = 0; subresource < count; ++subresource)
                    imageStates[i].push_back(input.initial.At(subresource));
            }
        }
        return {};
    }
    void VulkanGraphResources::BufferBarrier(
        uint32_t index, CopyBufferState target, VulkanBarriers &out, VkPipelineStageFlags2 stages, VkAccessFlags2 access)
    {
        index                       = bindings.bufferBacking[index];
        const auto             from = bufferStates[index];
        const auto             to   = BufferAccess(target);
        VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
        barrier.srcStageMask        = from.stage;
        barrier.srcAccessMask       = from.access;
        barrier.dstStageMask        = stages ? stages : to.stage;
        barrier.dstAccessMask       = access ? access : to.access;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer                                            = buffers[index]->GetNativeHandle();
        barrier.size                                              = VK_WHOLE_SIZE;
        out.buffers.push_back(barrier);
        bufferStates[index] = {barrier.dstStageMask, barrier.dstAccessMask};
    }
    void VulkanGraphResources::ImageBarrier(uint32_t index, ImageLayout target, VulkanBarriers &out, const ImageSubRange &range)
    {
        index              = bindings.imageBacking[index];
        const auto &desc   = images[index]->GetDescriptor();
        auto       &states = imageStates[index];
        for (uint32_t layer = range.baseLayer; layer < range.baseLayer + range.layers; ++layer) {
            for (uint32_t mip = range.baseLevel; mip < range.baseLevel + range.levels;) {
                const auto old   = states[layer * desc.mipLevels + mip];
                const auto first = mip++;
                while (mip < range.baseLevel + range.levels && states[layer * desc.mipLevels + mip] == old)
                    ++mip;
                if (old == target && target == ImageLayout::SHADER_READ_ONLY)
                    continue;
                const auto            from = ImageAccess(old), to = ImageAccess(target);
                VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                barrier.srcStageMask        = from.stage;
                barrier.srcAccessMask       = from.access;
                barrier.dstStageMask        = to.stage;
                barrier.dstAccessMask       = to.access;
                barrier.oldLayout           = FromImageLayout(old);
                barrier.newLayout           = FromImageLayout(target);
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.image                                             = images[index]->GetNativeHandle();
                barrier.subresourceRange                                  = {
                    static_cast<VkImageAspectFlags>(desc.format == PixelFormat::D32 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), first,
                    mip - first, layer, 1};
                out.images.push_back(barrier);
                std::fill(states.begin() + layer * desc.mipLevels + first, states.begin() + layer * desc.mipLevels + mip, target);
            }
        }
    }
    void VulkanGraphResources::TransferInputs(const FramePassResourceUses &uses, VulkanBarriers &out)
    {
        std::vector<bool> used(buffers.size());
        for (const auto &use : uses.buffers) {
            const auto physical = bindings.bufferBacking[use.range.buffer.index];
            if (!used[physical])
                BufferBarrier(use.range.buffer.index,
                              use.kind == FrameBufferUseKind::COPY_SOURCE ? CopyBufferState::COPY_SOURCE : CopyBufferState::COPY_DESTINATION, out);
            used[physical] = true;
        }
        for (const auto &use : uses.images)
            ImageBarrier(use.image.index, use.kind == FrameImageUseKind::COPY_SOURCE ? ImageLayout::TRANSFER_SRC : ImageLayout::TRANSFER_DST, out,
                         use.range);
    }
    std::variant<FrameError, VulkanTransferStep> VulkanGraphResources::Copy(const BufferCopyPass &pass, const FramePassResourceUses &uses)
    {
        auto *source      = buffers[pass.source.index];
        auto *destination = buffers[pass.destination.index];
        if (!source->GetDescriptor().usage.TestBit(BufferUsageFlagBit::TRANSFER_SRC) ||
            !destination->GetDescriptor().usage.TestBit(BufferUsageFlagBit::TRANSFER_DST))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        VulkanTransferStep step;
        TransferInputs(uses, step.before);
        step.source       = source->GetNativeHandle();
        step.destination  = destination->GetNativeHandle();
        step.bufferRegion = {pass.sourceOffset, pass.destinationOffset, pass.size};
        return step;
    }
    std::variant<FrameError, VulkanTransferStep> VulkanGraphResources::Copy(const BufferImageCopyPass &pass, const FramePassResourceUses &uses)
    {
        const bool upload = pass.direction == ImageCopyDirection::BUFFER_TO_IMAGE;
        auto      *buffer = buffers[pass.buffer.index];
        auto      *image  = images[pass.image.index];
        if (!buffer->GetDescriptor().usage.TestBit(upload ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST) ||
            !image->GetDescriptor().usage.TestBit(upload ? ImageUsageFlagBit::TRANSFER_DST : ImageUsageFlagBit::TRANSFER_SRC))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        VulkanTransferStep step;
        TransferInputs(uses, step.before);
        (upload ? step.source : step.destination) = buffer->GetNativeHandle();
        step.image                                = image->GetNativeHandle();
        step.imageRegion.bufferOffset             = pass.region.bufferOffset;
        step.imageRegion.bufferRowLength          = pass.region.bufferRowLength;
        step.imageRegion.bufferImageHeight        = pass.region.bufferImageHeight;
        step.imageRegion.imageOffset              = {pass.region.imageOffset.x, pass.region.imageOffset.y, 0};
        step.imageRegion.imageSubresource         = {VK_IMAGE_ASPECT_COLOR_BIT, pass.region.subRange.level, pass.region.subRange.baseLayer, 1};
        step.imageRegion.imageExtent              = {pass.region.imageExtent.width, pass.region.imageExtent.height, 1};
        return step;
    }
    void VulkanGraphResources::Finish(const FrameGraphAnalysis &analysis, VulkanBarriers &out)
    {
        for (auto index : analysis.usedBuffers)
            if (bindings.bufferBacking[index] == index)
                BufferBarrier(index, data.buffers[index].final, out);
        for (auto index : analysis.usedImages)
            if (bindings.imageBacking[index] == index) {
                const auto &desc = data.images[index].descriptor;
                for (uint32_t layer = 0; layer < desc.arrayLayers; ++layer)
                    for (uint32_t mip = 0; mip < desc.mipLevels;) {
                        const auto target = data.images[index].final.At(layer * desc.mipLevels + mip);
                        const auto first  = mip++;
                        while (mip < desc.mipLevels && data.images[index].final.At(layer * desc.mipLevels + mip) == target)
                            ++mip;
                        ImageBarrier(index, target, out, {first, mip - first, layer, 1});
                    }
            }
    }
    std::optional<FrameError> VulkanGraphResources::RasterInputs(const FramePassResourceUses &uses, VulkanBarriers &out)
    {
        std::vector<VulkanResourceAccess> reads(buffers.size());
        for (const auto &use : uses.buffers) {
            const auto index = use.range.buffer.index;
            if (!buffers[index]->GetDescriptor().usage.TestBit(FrameBufferUseUsage(use.kind)))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Raster buffer lacks its declared usage"};
            auto &read = reads[bindings.bufferBacking[index]];
            if (use.kind == FrameBufferUseKind::VERTEX || use.kind == FrameBufferUseKind::INDEX) {
                read.stage |= VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
                read.access |= use.kind == FrameBufferUseKind::VERTEX ? VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT : VK_ACCESS_2_INDEX_READ_BIT;
            } else {
                if (!use.stages || use.stages.TestBit(ShaderStageFlagBit::VS))
                    read.stage |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
                if (!use.stages || use.stages.TestBit(ShaderStageFlagBit::FS))
                    read.stage |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                read.access |= use.kind == FrameBufferUseKind::UNIFORM ? VK_ACCESS_2_UNIFORM_READ_BIT : VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
            }
        }
        for (uint32_t i = 0; i < reads.size(); ++i)
            if (reads[i].access)
                BufferBarrier(i, CopyBufferState::RASTER_READ, out, reads[i].stage, reads[i].access);
        for (const auto &use : uses.images)
            if (use.kind == FrameImageUseKind::SAMPLED) {
                if (!images[use.image.index]->GetDescriptor().usage.TestBit(ImageUsageFlagBit::SAMPLED))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid sampled image usage"};
                ImageBarrier(use.image.index, ImageLayout::SHADER_READ_ONLY, out, use.range);
            }
        return {};
    }
    VulkanGraphImageView::~VulkanGraphImageView()
    {
        if (view)
            device.GetDeviceFn().vkDestroyImageView(device.GetNativeHandle(), view, nullptr);
    }
    std::optional<FrameError> VulkanGraphResources::ShaderInputs(const FramePassResourceUses &uses, VulkanBarriers &out)
    {
        std::vector<VulkanResourceAccess> accesses(buffers.size());
        std::vector<CopyBufferState>      states(buffers.size(), CopyBufferState::SHADER_READ);
        for (const auto &use : uses.buffers) {
            const auto logical = use.range.buffer.index;
            if (!buffers[logical]->GetDescriptor().usage.TestBit(FrameBufferUseUsage(use.kind)))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader buffer lacks its declared usage"};
            const auto physical = bindings.bufferBacking[logical];
            auto      &access   = accesses[physical];
            if (!use.stages)
                access.stage |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
            if (use.stages.TestBit(ShaderStageFlagBit::VS))
                access.stage |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
            if (use.stages.TestBit(ShaderStageFlagBit::FS))
                access.stage |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            if (use.stages.TestBit(ShaderStageFlagBit::CS))
                access.stage |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
            if (use.kind == FrameBufferUseKind::UNIFORM)
                access.access |= VK_ACCESS_2_UNIFORM_READ_BIT;
            else {
                if (use.access != FrameAccessMode::WRITE)
                    access.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
                if (use.access != FrameAccessMode::READ)
                    access.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
                if (use.resourceAccess != ShaderResourceAccess::READ_ONLY)
                    states[physical] = CopyBufferState::UNORDERED_ACCESS;
            }
        }
        for (uint32_t i = 0; i < accesses.size(); ++i)
            if (accesses[i].access)
                BufferBarrier(i, states[i], out, accesses[i].stage, accesses[i].access);
        for (const auto &use : uses.images) {
            const bool storage = use.kind == FrameImageUseKind::STORAGE;
            if (!images[use.image.index]->GetDescriptor().usage.TestBit(storage ? ImageUsageFlagBit::STORAGE : ImageUsageFlagBit::SAMPLED))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Shader image lacks its declared usage"};
            ImageBarrier(use.image.index, storage ? ImageLayout::GENERAL : ImageLayout::SHADER_READ_ONLY, out, use.range);
        }
        return {};
    }
    std::variant<FrameError, VulkanFrameBindings> VulkanGraphResources::ResolveBindings(VulkanDeviceFrameContext &context,
                                                                                        const DeviceFrameLease   &lease,
                                                                                        VulkanShader             &shader,
                                                                                        const FrameBindingGroups &groups)
    {
        VulkanFrameBindings result;
        for (uint32_t logicalSet = 0; logicalSet < groups.size(); ++logicalSet) {
            const auto &use = groups[logicalSet];
            if (!use.group)
                continue;
            const auto &group      = use.group->GetData();
            const auto  findBuffer = [&](uint32_t binding) {
                return std::ranges::find_if(group.buffers, [&](const auto &value) { return value.binding == binding; });
            };
            const auto dynamicOffset = [&](uint32_t binding) {
                const auto found = std::ranges::find_if(use.bufferUses, [&](const auto &value) { return value.binding == binding; });
                return found == use.bufferUses.end() ? 0u : found->dynamicOffset;
            };
            const auto [entry, inserted] = bindingGroups.try_emplace(use.group.get());
            auto &snapshot               = entry->second;
            for (const auto &[physicalSet, setPlan] : shader.GetBindingPlan().sets) {
                if (setPlan.logicalSet != logicalSet)
                    continue;
                if (inserted) {
                    auto allocation = context.AllocateDescriptorSet(lease, shader, physicalSet);
                    if (const auto *error = std::get_if<FrameError>(&allocation))
                        return *error;
                    VulkanDescriptorSet native{physicalSet, std::get<VkDescriptorSet>(allocation)};
                    for (const auto &planned : setPlan.bindings) {
                        if (planned.count != 1)
                            return FrameError{FrameErrorCode::UNSUPPORTED, "Graph descriptor arrays are not supported"};
                        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                        write.dstSet          = native.descriptor;
                        write.dstBinding      = planned.physicalBinding;
                        write.descriptorCount = 1;
                        write.descriptorType  = FromShaderResourceType(planned.type);
                        VkDescriptorBufferInfo buffer{};
                        VkDescriptorImageInfo  image{};
                        const bool             uniform =
                            planned.type == ShaderResourceType::UNIFORM_BUFFER || planned.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                        const bool storage =
                            planned.type == ShaderResourceType::STORAGE_BUFFER || planned.type == ShaderResourceType::STORAGE_BUFFER_DYNAMIC;
                        if (uniform || storage) {
                            const auto binding = findBuffer(planned.logicalBinding);
                            if (binding == group.buffers.end())
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing logical buffer binding"};
                            const auto *resource  = buffers[binding->range.buffer.index];
                            const auto  alignment = std::max<VkDeviceSize>(1, uniform ? device.GetLimits().minUniformBufferOffsetAlignment
                                                                                      : device.GetLimits().minStorageBufferOffsetAlignment);
                            const auto  maximum   = uniform ? device.GetLimits().maxUniformBufferRange : device.GetLimits().maxStorageBufferRange;
                            if (!resource->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE) ||
                                binding->range.offset % alignment || binding->range.size > maximum)
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid Vulkan buffer usage/alignment/range"};
                            buffer            = {resource->GetNativeHandle(), binding->range.offset, binding->range.size};
                            write.pBufferInfo = &buffer;
                        } else if (planned.type == ShaderResourceType::SAMPLED_IMAGE || planned.type == ShaderResourceType::STORAGE_IMAGE) {
                            const auto binding =
                                std::ranges::find_if(group.images, [&](const auto &value) { return value.binding == planned.logicalBinding; });
                            if (binding == group.images.end())
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing logical image binding"};
                            const bool storageImage = planned.type == ShaderResourceType::STORAGE_IMAGE;
                            auto       normalized = ResolveDeclaredView({logicalSet, binding->binding, binding->image, binding->view}, storageImage);
                            if (const auto *error = std::get_if<FrameError>(&normalized))
                                return *error;
                            auto view = ImageView(binding->image.index, std::get<RasterImageView>(normalized));
                            if (const auto *error = std::get_if<FrameError>(&view))
                                return *error;
                            auto retained    = std::get<std::shared_ptr<VulkanGraphImageView>>(std::move(view));
                            image            = {VK_NULL_HANDLE, retained->view,
                                     storageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                            write.pImageInfo = &image;
                            snapshot.views.push_back(std::move(retained));
                        } else if (planned.type == ShaderResourceType::SAMPLER) {
                            const auto binding =
                                std::ranges::find_if(group.samplers, [&](const auto &value) { return value.binding == planned.logicalBinding; });
                            if (binding == group.samplers.end() || !binding->sampler || binding->sampler->GetDevice() != &device)
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing or foreign logical sampler"};
                            const auto handle = static_cast<const VulkanSampler *>(binding->sampler.Get())->GetNativeHandle();
                            if (!handle)
                                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid Vulkan sampler"};
                            image            = {handle, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
                            write.pImageInfo = &image;
                        } else
                            return FrameError{FrameErrorCode::UNSUPPORTED, "Unsupported Vulkan descriptor kind"};
                        device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
                    }
                    snapshot.sets.push_back(std::move(native));
                }
            }
            for (const auto &materialized : snapshot.sets) {
                auto native = materialized;
                for (const auto &planned : shader.GetBindingPlan().sets.at(native.set).bindings) {
                    const bool uniform = planned.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                    if (!uniform && planned.type != ShaderResourceType::STORAGE_BUFFER_DYNAMIC)
                        continue;
                    const auto offset    = dynamicOffset(planned.logicalBinding);
                    const auto alignment = std::max<VkDeviceSize>(1, uniform ? device.GetLimits().minUniformBufferOffsetAlignment
                                                                             : device.GetLimits().minStorageBufferOffsetAlignment);
                    if (offset % alignment)
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Misaligned Vulkan dynamic buffer offset"};
                    native.offsets.push_back(offset);
                }
                result.sets.push_back(std::move(native));
            }
            result.views.insert(result.views.end(), snapshot.views.begin(), snapshot.views.end());
        }
        std::sort(result.sets.begin(), result.sets.end(), [](const auto &a, const auto &b) { return a.set < b.set; });
        return result;
    }
    std::variant<FrameError, RasterImageView> VulkanGraphResources::ResolveDeclaredView(const FrameImageBinding &binding, bool storage) const
    {
        return ResolveFrameImageView(binding, images[binding.image.index]->GetDescriptor(), storage);
    }
    std::variant<FrameError, std::shared_ptr<VulkanGraphImageView>> VulkanGraphResources::AttachmentView(GraphImageHandle            image,
                                                                                                         const RasterAttachmentView &view)
    {
        auto resolved = ResolveRasterAttachmentView(view, images[image.index]->GetDescriptor());
        if (auto *error = std::get_if<FrameError>(&resolved))
            return std::move(*error);
        return ImageView(image.index, {ImageViewType::VIEW_2D, view.mip, 1, view.layer, 1});
    }
    std::variant<FrameError, std::shared_ptr<VulkanGraphImageView>> VulkanGraphResources::ImageView(uint32_t index, const RasterImageView &range)
    {
        auto         *image = images[index];
        const ViewKey key{bindings.imageBacking[index], range.type, range.baseMip, range.mipCount, range.baseLayer, range.layerCount};
        if (const auto found = imageViews.find(key); found != imageViews.end())
            return found->second;
        auto                  view = std::make_shared<VulkanGraphImageView>(device);
        VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        info.image            = image->GetNativeHandle();
        info.format           = image->GetVkFormat();
        info.viewType         = range.type == ImageViewType::VIEW_2D_ARRAY ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        info.subresourceRange = {
            VkImageAspectFlags(image->GetDescriptor().format == PixelFormat::D32 ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            range.baseMip, range.mipCount, range.baseLayer, range.layerCount};
        VkImageView nativeView = VK_NULL_HANDLE;
        const auto  result     = device.GetDeviceFn().vkCreateImageView(device.GetNativeHandle(), &info, nullptr, &nativeView);
        if (result != VK_SUCCESS)
            return VulkanFrameError(result, FrameErrorCode::COMPILE_FAILED);
        view->view = nativeView;
        imageViews.emplace(key, view);
        return view;
    }
    std::variant<FrameError, std::vector<VulkanDraw>> ResolveVulkanDraws(VulkanDevice             &device,
                                                                         VulkanDeviceFrameContext &context,
                                                                         const DeviceFrameLease   &lease,
                                                                         VulkanGraphResources     &resources,
                                                                         const FrameRasterPass    &pass,
                                                                         VkExtent2D                extent,
                                                                         PixelFormat               format,
                                                                         const SubpassInfo        *subpass)
    {
        std::vector<VulkanDraw> result;
        auto                    less = [](const RasterDraw *a, const RasterDraw *b) { return RasterPipelineLess(*a, *b); };
        std::map<const RasterDraw *, CounterPtr<VulkanGraphicsPipeline>, decltype(less)> pipelines(less);
        for (const auto &draw : pass.draws) {
            if (!draw.shader || draw.shader->GetDevice() != &device)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto *shader = static_cast<VulkanShader *>(draw.shader.Get());
            if (!shader->GetVertexFunction() || !shader->GetFragmentFunction() || shader->GetComputeFunction())
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            if (shader->GetVertexFunction()->GetStage() != ShaderStageFlagBit::VS ||
                shader->GetFragmentFunction()->GetStage() != ShaderStageFlagBit::FS)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            const auto &blend = draw.blend;
            if (blend.srcColor >= BlendFactor::SRC1_COLOR || blend.dstColor >= BlendFactor::SRC1_COLOR || blend.srcAlpha >= BlendFactor::SRC1_COLOR ||
                blend.dstAlpha >= BlendFactor::SRC1_COLOR)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Dual-source blending is not implemented"};
            auto                         state = RasterPipelineState(draw);
            GraphicsPipeline::Descriptor descriptor;
            descriptor.state              = &state;
            descriptor.shader             = shader;
            const bool hasColor           = pass.color != GraphImageHandle{};
            descriptor.format.sampleCount = resources.images[hasColor ? pass.color.index : pass.depth->image.index]->GetDescriptor().samples;
            state.multiSample.sampleCount = descriptor.format.sampleCount;
            descriptor.format.numColors   = hasColor ? 1 : 0;
            if (!hasColor)
                state.blendStates.clear();
            descriptor.format.colors[0]    = format;
            descriptor.format.depthStencil = pass.depth ? PixelFormat::D32 : PixelFormat::UNDEFINED;
            auto &pipeline                 = pipelines[&draw];
            if (!pipeline) {
                auto cached = device.GetFramePipelineCache().Acquire(
                    FramePipelineKey{draw, descriptor.format, lease.context.deviceEpoch, subpass ? subpass->cacheIdentity : 0,
                                     subpass ? subpass->subpass : 0},
                    [&]() -> std::variant<FrameError, CounterPtr<GraphicsPipeline>> {
                        CounterPtr<VulkanGraphicsPipeline> created(new VulkanGraphicsPipeline(device));
                        if (!(subpass ? created->Init(descriptor, *subpass) : created->Init(descriptor))) {
                            if (created->GetCreateResult() != VK_SUCCESS)
                                return VulkanFrameError(created->GetCreateResult(), FrameErrorCode::COMPILE_FAILED);
                            return FrameError{FrameErrorCode::COMPILE_FAILED, "Graphics pipeline creation failed"};
                        }
                        return CounterPtr<GraphicsPipeline>(created.Get());
                    });
                if (auto *error = std::get_if<FrameError>(&cached))
                    return std::move(*error);
                pipeline.Reset(static_cast<VulkanGraphicsPipeline *>(std::get<CounterPtr<GraphicsPipeline>>(cached).Get()));
            }
            VkRect2D scissor{{draw.scissor.offset.x, draw.scissor.offset.y}, {draw.scissor.extent.width, draw.scissor.extent.height}};
            if (!scissor.extent.width)
                scissor.extent = extent;
            result.push_back({pipeline, draw.command, scissor});
            auto &native  = result.back();
            native.layout = shader->GetPipelineLayout();
            for (const auto &stream : draw.vertices) {
                native.vertices.push_back(resources.buffers[stream.range.buffer.index]->GetNativeHandle());
                native.vertexOffsets.push_back(stream.range.offset);
            }
            if (draw.indexed) {
                native.index       = resources.buffers[draw.indexed->range.buffer.index]->GetNativeHandle();
                native.indexOffset = draw.indexed->range.offset;
                native.indexType   = draw.indexed->type == IndexType::U16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
                native.indexed     = draw.indexed->command;
            }
            auto bindings = resources.ResolveBindings(context, lease, *shader, draw.groups);
            if (const auto *error = std::get_if<FrameError>(&bindings))
                return *error;
            auto resolved = std::get<VulkanFrameBindings>(std::move(bindings));
            native.sets   = std::move(resolved.sets);
            native.views  = std::move(resolved.views);
        }
        return result;
    }
    std::variant<FrameError, VulkanComputeStep> ResolveVulkanCompute(VulkanDevice                &device,
                                                                     VulkanDeviceFrameContext    &context,
                                                                     const DeviceFrameLease      &lease,
                                                                     VulkanGraphResources        &resources,
                                                                     const FrameComputePass      &pass,
                                                                     const FramePassResourceUses &uses)
    {
        if (!pass.shader || pass.shader->GetDevice() != &device)
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute shader belongs to another device"};
        auto *shader = static_cast<VulkanShader *>(pass.shader.Get());
        if (!shader->GetComputeFunction() || shader->GetVertexFunction() || shader->GetFragmentFunction())
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute pass requires a compute-only shader"};
        const auto &limits = device.GetLimits();
        if (pass.groupsX > limits.maxComputeWorkGroupCount[0] || pass.groupsY > limits.maxComputeWorkGroupCount[1] ||
            pass.groupsZ > limits.maxComputeWorkGroupCount[2])
            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute dispatch exceeds device limits"};
        VulkanComputeStep step;
        step.groupsX = pass.groupsX;
        step.groupsY = pass.groupsY;
        step.groupsZ = pass.groupsZ;
        step.layout  = shader->GetPipelineLayout();
        CounterPtr<VulkanComputePipeline> pipeline(new VulkanComputePipeline(device));
        if (!pipeline->Init(ComputePipeline::Descriptor{shader}))
            return FrameError{FrameErrorCode::COMPILE_FAILED, "Compute pipeline creation failed"};
        step.pipeline = std::move(pipeline);

        auto bindings = resources.ResolveBindings(context, lease, *shader, pass.groups);
        if (const auto *error = std::get_if<FrameError>(&bindings))
            return *error;
        auto resolved = std::get<VulkanFrameBindings>(std::move(bindings));
        step.sets     = std::move(resolved.sets);
        step.views    = std::move(resolved.views);
        if (const auto error = resources.ShaderInputs(uses, step.before))
            return *error;
        return step;
    }
    std::variant<FrameError, VulkanCustomStep> VulkanGraphResources::Custom(const FrameCustomPass &pass, const FramePassResourceUses &uses)
    {
        VulkanCustomStep step;
        step.encoder = pass.encoder;
        if (const auto error = ShaderInputs(uses, step.before))
            return *error;
        for (const auto &use : uses.buffers)
            step.buffers.push_back(buffers[use.range.buffer.index]);
        for (const auto &use : uses.images)
            step.images.push_back(images[use.image.index]);
        return step;
    }
    void VulkanComputeStep::Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command) const noexcept
    {
        before.Encode(fn, command);
        fn.vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->GetNativeHandle());
        for (const auto &set : sets)
            fn.vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, layout, set.set, 1, &set.descriptor,
                                       static_cast<uint32_t>(set.offsets.size()), set.offsets.data());
        fn.vkCmdDispatch(command, groupsX, groupsY, groupsZ);
    }
    FrameOperationResult VulkanCustomStep::Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command, CommandBuffer &rhi) const noexcept
    {
        before.Encode(fn, command);
        FrameCustomPassContext context{rhi, std::span<Image *const>(images), std::span<Buffer *const>(buffers)};
        return encoder->Encode(context);
    }
    void EncodeVulkanDraws(const VulkanDeviceFunctions &fn, VkCommandBuffer command, VkExtent2D extent, std::span<const VulkanDraw> draws) noexcept
    {
        VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
        fn.vkCmdSetViewport(command, 0, 1, &viewport);
        for (const auto &draw : draws) {
            fn.vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.pipeline->GetNativeHandle());
            fn.vkCmdSetScissor(command, 0, 1, &draw.scissor);
            for (const auto &set : draw.sets)
                fn.vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.layout, set.set, 1, &set.descriptor,
                                           static_cast<uint32_t>(set.offsets.size()), set.offsets.data());
            if (!draw.vertices.empty())
                fn.vkCmdBindVertexBuffers(command, 0, static_cast<uint32_t>(draw.vertices.size()), draw.vertices.data(), draw.vertexOffsets.data());
            if (draw.index) {
                fn.vkCmdBindIndexBuffer(command, draw.index, draw.indexOffset, draw.indexType);
                fn.vkCmdDrawIndexed(command, draw.indexed.indexCount, draw.indexed.instanceCount, draw.indexed.firstIndex, draw.indexed.vertexOffset,
                                    draw.indexed.firstInstance);
            } else
                fn.vkCmdDraw(command, draw.command.vertexCount, draw.command.instanceCount, draw.command.firstVertex, draw.command.firstInstance);
        }
    }
} // namespace sky::aurora
