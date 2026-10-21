#include "VulkanGraphResources.h"
#include <aurora/rdg/FrameBindingLookup.h>

#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanInstance.h>
#include <VulkanSampler.h>
#include <VulkanShader.h>
#include <algorithm>
#include <common/FrameRasterBindings.h>
#include <map>
#include <rdg/VulkanDeviceFrameContext.h>

namespace sky::aurora {
    namespace {
        struct Access {
            VkPipelineStageFlags2 stage;
            VkAccessFlags2        access;
        };
        Access BufferAccess(CopyBufferState state)
        {
            switch (state) {
            case CopyBufferState::HOST_WRITE: return {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_WRITE_BIT};
            case CopyBufferState::HOST_READ: return {VK_PIPELINE_STAGE_2_HOST_BIT, VK_ACCESS_2_HOST_READ_BIT};
            case CopyBufferState::COPY_SOURCE: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT};
            case CopyBufferState::COPY_DESTINATION: return {VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT};
            case CopyBufferState::RASTER_READ:
                return {VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_2_INDEX_READ_BIT | VK_ACCESS_2_UNIFORM_READ_BIT};
            case CopyBufferState::SHADER_READ:
                return {VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_SHADER_SAMPLED_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_READ_BIT};
            case CopyBufferState::UNORDERED_ACCESS:
                // Custom may use storage from graphics; the state must cover both sides of that pass.
                return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT};
            default: return {VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT};
            }
        }
        Access ImageAccess(ImageLayout state)
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
            bufferStates[i] = input.initial;
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
    void VulkanGraphResources::BufferBarrier(uint32_t index, CopyBufferState target, VulkanBarriers &out)
    {
        index                       = bindings.bufferBacking[index];
        const auto             from = BufferAccess(bufferStates[index]);
        const auto             to   = BufferAccess(target);
        VkBufferMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
        barrier.srcStageMask        = from.stage;
        barrier.srcAccessMask       = from.access;
        barrier.dstStageMask        = to.stage;
        barrier.dstAccessMask       = to.access;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer                                            = buffers[index]->GetNativeHandle();
        barrier.size                                              = VK_WHOLE_SIZE;
        out.buffers.push_back(barrier);
        bufferStates[index] = target;
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
    std::variant<FrameError, VulkanTransferStep> VulkanGraphResources::Copy(const BufferCopyPass &pass)
    {
        auto *source      = buffers[pass.source.index];
        auto *destination = buffers[pass.destination.index];
        if (!source->GetDescriptor().usage.TestBit(BufferUsageFlagBit::TRANSFER_SRC) ||
            !destination->GetDescriptor().usage.TestBit(BufferUsageFlagBit::TRANSFER_DST))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        VulkanTransferStep step;
        BufferBarrier(pass.source.index, CopyBufferState::COPY_SOURCE, step.before);
        BufferBarrier(pass.destination.index, CopyBufferState::COPY_DESTINATION, step.before);
        step.source       = source->GetNativeHandle();
        step.destination  = destination->GetNativeHandle();
        step.bufferRegion = {pass.sourceOffset, pass.destinationOffset, pass.size};
        return step;
    }
    std::variant<FrameError, VulkanTransferStep> VulkanGraphResources::Copy(const BufferImageCopyPass &pass)
    {
        const bool upload = pass.direction == ImageCopyDirection::BUFFER_TO_IMAGE;
        auto      *buffer = buffers[pass.buffer.index];
        auto      *image  = images[pass.image.index];
        if (!buffer->GetDescriptor().usage.TestBit(upload ? BufferUsageFlagBit::TRANSFER_SRC : BufferUsageFlagBit::TRANSFER_DST) ||
            !image->GetDescriptor().usage.TestBit(upload ? ImageUsageFlagBit::TRANSFER_DST : ImageUsageFlagBit::TRANSFER_SRC))
            return FrameError{FrameErrorCode::INVALID_ARGUMENT};
        VulkanTransferStep step;
        BufferBarrier(pass.buffer.index, upload ? CopyBufferState::COPY_SOURCE : CopyBufferState::COPY_DESTINATION, step.before);
        ImageBarrier(pass.image.index, upload ? ImageLayout::TRANSFER_DST : ImageLayout::TRANSFER_SRC, step.before,
                     {pass.region.subRange.level, 1, pass.region.subRange.baseLayer, 1});
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
    std::optional<FrameError> VulkanGraphResources::RasterInputs(const FrameRasterPass &pass, VulkanBarriers &out)
    {
        std::vector<bool> used(buffers.size());
        auto              read = [&](GraphBufferHandle handle, BufferUsageFlagBit usage) {
            if (!buffers[handle.index]->GetDescriptor().usage.TestBit(usage))
                return false;
            used[handle.index] = true;
            return true;
        };
        for (const auto &draw : pass.draws) {
            for (const auto &stream : draw.vertices)
                if (!read(stream.range.buffer, BufferUsageFlagBit::VERTEX))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing vertex buffer usage"};
            if (draw.indexed && !read(draw.indexed->range.buffer, BufferUsageFlagBit::INDEX))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing index buffer usage"};
            for (const auto &binding : draw.uniforms)
                if (!read(binding.range.buffer, BufferUsageFlagBit::UNIFORM))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing uniform buffer usage"};
            for (const auto &binding : draw.images) {
                const auto *image = images[binding.image.index];
                if (!image->GetDescriptor().usage.TestBit(ImageUsageFlagBit::SAMPLED))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid sampled image usage"};
                const auto resolved = ResolveRasterImageView(binding.view, image->GetDescriptor());
                if (const auto *error = std::get_if<FrameError>(&resolved))
                    return *error;
                const auto &view = std::get<RasterImageView>(resolved);
                ImageBarrier(binding.image.index, ImageLayout::SHADER_READ_ONLY, out, {view.baseMip, view.mipCount, view.baseLayer, view.layerCount});
            }
        }
        for (uint32_t i = 0; i < used.size(); ++i)
            if (used[i])
                BufferBarrier(i, CopyBufferState::RASTER_READ, out);
        return {};
    }
    VulkanGraphImageView::~VulkanGraphImageView()
    {
        if (view)
            device.GetDeviceFn().vkDestroyImageView(device.GetNativeHandle(), view, nullptr);
    }
    std::variant<FrameError, std::shared_ptr<VulkanGraphImageView>> VulkanGraphResources::SampledView(const RasterImageBinding &binding)
    {
        auto *image    = images[binding.image.index];
        auto  resolved = ResolveRasterImageView(binding.view, image->GetDescriptor());
        if (auto *error = std::get_if<FrameError>(&resolved))
            return std::move(*error);
        return ImageView(binding.image.index, std::get<RasterImageView>(resolved));
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
        std::map<const RasterDraw *, size_t, FrameRasterBindingsLess>                    descriptors;
        for (const auto &draw : pass.draws) {
            if (!draw.shader || draw.shader->GetDevice() != &device)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            auto *shader = static_cast<VulkanShader *>(draw.shader.Get());
            if (!shader->GetVertexFunction() || !shader->GetFragmentFunction() || shader->GetComputeFunction())
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            if (shader->GetVertexFunction()->GetStage() != ShaderStageFlagBit::VS ||
                shader->GetFragmentFunction()->GetStage() != ShaderStageFlagBit::FS)
                return FrameError{FrameErrorCode::INVALID_ARGUMENT};
            if (auto error = ValidateRasterBindings(draw, shader->GetReflection()))
                return std::move(*error);
            const FrameBindingLookup<ShaderResource> reflection(shader->GetReflection().resources);
            const auto                              &blend = draw.blend;
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
            const auto [cachedBindings, inserted] = descriptors.emplace(&draw, result.size() - 1);
            if (!inserted) {
                const auto &previous = result[cachedBindings->second];
                native.sets          = previous.sets;
                native.views         = previous.views;
                continue;
            }
            for (uint32_t set = 0; set < 3; ++set) {
                std::vector<const RasterUniformBinding *> bindings;
                for (const auto &binding : draw.uniforms)
                    if (binding.set == set)
                        bindings.push_back(&binding);
                if (bindings.empty() && std::none_of(draw.images.begin(), draw.images.end(), [&](const auto &b) { return b.set == set; }) &&
                    std::none_of(draw.samplers.begin(), draw.samplers.end(), [&](const auto &b) { return b.set == set; }))
                    continue;
                std::sort(bindings.begin(), bindings.end(), [](auto a, auto b) { return a->binding < b->binding; });
                auto allocated = context.AllocateDescriptorSet(lease, *shader, set);
                if (auto *error = std::get_if<FrameError>(&allocated))
                    return std::move(*error);
                VulkanDescriptorSet resolved{set, std::get<VkDescriptorSet>(allocated)};
                for (const auto *binding : bindings) {
                    const auto alignment = std::max(1u, device.GetCapability().minUniformBufferOffsetAlignment);
                    if (binding->range.offset % alignment || binding->dynamicOffset % alignment ||
                        binding->range.size > device.GetLimits().maxUniformBufferRange)
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid Vulkan UBO alignment/range"};
                    const auto *resource = reflection.Find(set, binding->binding);
                    if (!resource)
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing raster uniform reflection"};
                    const bool             dynamic = resource->type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
                    VkDescriptorBufferInfo buffer{resources.buffers[binding->range.buffer.index]->GetNativeHandle(), binding->range.offset,
                                                  binding->range.size};
                    VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    write.dstSet          = resolved.descriptor;
                    write.dstBinding      = binding->binding;
                    write.descriptorCount = 1;
                    write.descriptorType  = dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                    write.pBufferInfo     = &buffer;
                    device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
                    if (dynamic)
                        resolved.offsets.push_back(binding->dynamicOffset);
                }
                auto writeImage = [&](uint32_t binding, VkDescriptorType type, const VkDescriptorImageInfo &image) {
                    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    write.dstSet          = resolved.descriptor;
                    write.dstBinding      = binding;
                    write.descriptorCount = 1;
                    write.descriptorType  = type;
                    write.pImageInfo      = &image;
                    device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
                };
                for (const auto &binding : draw.images)
                    if (binding.set == set) {
                        auto resolvedView = resources.SampledView(binding);
                        if (auto *error = std::get_if<FrameError>(&resolvedView))
                            return std::move(*error);
                        auto view = std::get<std::shared_ptr<VulkanGraphImageView>>(std::move(resolvedView));
                        writeImage(binding.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                   {VK_NULL_HANDLE, view->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
                        native.views.push_back(std::move(view));
                    }
                for (const auto &binding : draw.samplers)
                    if (binding.set == set) {
                        if (binding.sampler->GetDevice() != &device)
                            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Sampler belongs to another or unknown device"};
                        auto sampler = static_cast<const VulkanSampler *>(binding.sampler.Get())->GetNativeHandle();
                        if (!sampler)
                            return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid Vulkan sampler"};
                        writeImage(binding.binding, VK_DESCRIPTOR_TYPE_SAMPLER, {sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED});
                    }
                native.sets.push_back(std::move(resolved));
            }
        }
        return result;
    }
    std::variant<FrameError, VulkanComputeStep> ResolveVulkanCompute(VulkanDevice             &device,
                                                                     VulkanDeviceFrameContext &context,
                                                                     const DeviceFrameLease   &lease,
                                                                     VulkanGraphResources     &resources,
                                                                     const FrameComputePass   &pass)
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

        const FrameBindingLookup<ShaderResource> reflection(shader->GetReflection().resources);
        for (uint32_t set = 0; set < 3; ++set) {
            if (!reflection.HasSet(set))
                continue;
            auto allocated = context.AllocateDescriptorSet(lease, *shader, set);
            if (auto *error = std::get_if<FrameError>(&allocated))
                return std::move(*error);
            VulkanDescriptorSet resolved{set, std::get<VkDescriptorSet>(allocated)};
            resolved.offsets = ResolveComputeDynamicOffsets(pass, set);
            for (const auto &binding : pass.buffers) {
                if (binding.set != set)
                    continue;
                const auto *ref = reflection.Find(set, binding.binding);
                if (!ref)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing compute buffer reflection"};
                VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                switch (ref->type) {
                case ShaderResourceType::UNIFORM_BUFFER: type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; break;
                case ShaderResourceType::UNIFORM_BUFFER_DYNAMIC: type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC; break;
                case ShaderResourceType::STORAGE_BUFFER_DYNAMIC: type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC; break;
                case ShaderResourceType::STORAGE_BUFFER: break;
                default: return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute buffer binding type mismatch"};
                }
                const bool uniform = type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                const auto alignment =
                    std::max<VkDeviceSize>(1, uniform ? limits.minUniformBufferOffsetAlignment : limits.minStorageBufferOffsetAlignment);
                const auto  maximum = uniform ? limits.maxUniformBufferRange : limits.maxStorageBufferRange;
                const auto *buffer  = resources.buffers[binding.range.buffer.index];
                if (!buffer->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM : BufferUsageFlagBit::STORAGE) ||
                    binding.range.offset % alignment || binding.dynamicOffset % alignment || binding.range.size > maximum)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid compute buffer usage/alignment/range"};
                VkDescriptorBufferInfo info{resources.buffers[binding.range.buffer.index]->GetNativeHandle(), binding.range.offset,
                                            binding.range.size};
                VkWriteDescriptorSet   write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet          = resolved.descriptor;
                write.dstBinding      = binding.binding;
                write.descriptorCount = 1;
                write.descriptorType  = type;
                write.pBufferInfo     = &info;
                device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
                resources.BufferBarrier(binding.range.buffer.index, uniform ? CopyBufferState::SHADER_READ : CopyBufferState::UNORDERED_ACCESS,
                                        step.before);
            }
            for (const auto &binding : pass.images) {
                if (binding.set != set)
                    continue;
                const auto *ref = reflection.Find(set, binding.binding);
                if (!ref || (ref->type != ShaderResourceType::SAMPLED_IMAGE && ref->type != ShaderResourceType::STORAGE_IMAGE))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Missing compute image reflection"};
                const bool storage    = ref->type == ShaderResourceType::STORAGE_IMAGE;
                auto       normalized = resources.ResolveDeclaredView(binding, storage);
                if (auto *error = std::get_if<FrameError>(&normalized))
                    return std::move(*error);
                const auto &resolvedView = std::get<RasterImageView>(normalized);
                auto        view         = resources.ImageView(binding.image.index, resolvedView);
                if (auto *error = std::get_if<FrameError>(&view))
                    return std::move(*error);
                auto                  viewPtr = std::get<std::shared_ptr<VulkanGraphImageView>>(std::move(view));
                VkDescriptorImageInfo info{VK_NULL_HANDLE, viewPtr->view,
                                           storage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
                VkWriteDescriptorSet  write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet          = resolved.descriptor;
                write.dstBinding      = binding.binding;
                write.descriptorCount = 1;
                write.descriptorType  = storage ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                write.pImageInfo      = &info;
                device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
                resources.ImageBarrier(binding.image.index, storage ? ImageLayout::GENERAL : ImageLayout::SHADER_READ_ONLY, step.before,
                                       {resolvedView.baseMip, resolvedView.mipCount, resolvedView.baseLayer, resolvedView.layerCount});
                step.views.push_back(std::move(viewPtr));
            }
            for (const auto &binding : pass.samplers) {
                if (binding.set != set)
                    continue;
                if (binding.sampler->GetDevice() != &device)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Compute sampler belongs to another device"};
                const auto sampler = static_cast<const VulkanSampler *>(binding.sampler.Get())->GetNativeHandle();
                if (!sampler)
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Invalid compute sampler"};
                VkDescriptorImageInfo info{sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
                VkWriteDescriptorSet  write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                write.dstSet          = resolved.descriptor;
                write.dstBinding      = binding.binding;
                write.descriptorCount = 1;
                write.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
                write.pImageInfo      = &info;
                device.GetDeviceFn().vkUpdateDescriptorSets(device.GetNativeHandle(), 1, &write, 0, nullptr);
            }
            step.sets.push_back(std::move(resolved));
        }
        return step;
    }
    std::variant<FrameError, VulkanCustomStep> VulkanGraphResources::Custom(const FrameCustomPass &pass)
    {
        VulkanCustomStep step;
        step.encoder = pass.encoder;
        for (const auto &binding : pass.buffers) {
            const bool uniform = binding.kind == FrameBufferBindingKind::UNIFORM;
            if (!buffers[binding.range.buffer.index]->GetDescriptor().usage.TestBit(uniform ? BufferUsageFlagBit::UNIFORM
                                                                                            : BufferUsageFlagBit::STORAGE))
                return FrameError{FrameErrorCode::INVALID_ARGUMENT, "Custom buffer kind does not match usage"};
            BufferBarrier(binding.range.buffer.index, uniform ? CopyBufferState::SHADER_READ : CopyBufferState::UNORDERED_ACCESS, step.before);
            step.buffers.push_back(buffers[binding.range.buffer.index]);
        }
        for (const auto &binding : pass.images) {
            const auto view = ResolveDeclaredView(binding);
            if (const auto *error = std::get_if<FrameError>(&view))
                return *error;
            const auto &resolved = std::get<RasterImageView>(view);
            ImageBarrier(binding.image.index, binding.kind == FrameImageBindingKind::STORAGE ? ImageLayout::GENERAL : ImageLayout::SHADER_READ_ONLY,
                         step.before, {resolved.baseMip, resolved.mipCount, resolved.baseLayer, resolved.layerCount});
            step.images.push_back(images[binding.image.index]);
        }
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
