#include "VulkanFrameCompiler.h"
#include "VulkanGraphResources.h"
#include "VulkanRenderPassCache.h"
#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <algorithm>

namespace sky::aurora {
    namespace {
        struct RenderPassObjects {
            explicit RenderPassObjects(VulkanDevice &device) : device(device)
            {
            }
            ~RenderPassObjects()
            {
                const auto &fn = device.GetDeviceFn();
                if (framebuffer)
                    fn.vkDestroyFramebuffer(device.GetNativeHandle(), framebuffer, nullptr);
            }
            std::shared_ptr<VulkanGraphImageView>   colorView, depthView, resolveView, depthResolveView;
            VulkanDevice                           &device;
            VkRenderPass                            renderPass = VK_NULL_HANDLE;
            std::shared_ptr<VulkanCachedRenderPass> cachedPass;
            VkFramebuffer                           framebuffer = VK_NULL_HANDLE;
        };
        struct RenderPassStep {
            VulkanBarriers                       before;
            std::unique_ptr<RenderPassObjects>   objects;
            VkExtent2D                           extent{};
            std::array<VkClearValue, 4>          clear{};
            uint32_t                             attachmentCount = 1;
            std::vector<std::vector<VulkanDraw>> subpasses;
            std::vector<VulkanFragmentRange>     fragments;
        };
        class VulkanRenderPassEncoder final : public VulkanFrameEncoder {
        public:
            void Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command, std::span<const VkCommandBuffer> fragments) const noexcept override
            {
                for (const auto &step : steps) {
                    if (const auto *copy = std::get_if<VulkanTransferStep>(&step)) {
                        copy->Encode(fn, command);
                        continue;
                    }
                    const auto &raster = std::get<RenderPassStep>(step);
                    raster.before.Encode(fn, command);
                    VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
                    info.renderPass        = raster.objects->renderPass;
                    info.framebuffer       = raster.objects->framebuffer;
                    info.renderArea.extent = raster.extent;
                    info.clearValueCount   = raster.attachmentCount;
                    info.pClearValues      = raster.clear.data();
                    VkSubpassBeginInfo begin{VK_STRUCTURE_TYPE_SUBPASS_BEGIN_INFO};
                    begin.contents = fragments.empty() ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
                    fn.vkCmdBeginRenderPass2(command, &info, &begin);
                    VkSubpassEndInfo end{VK_STRUCTURE_TYPE_SUBPASS_END_INFO};
                    for (size_t subpass = 0; subpass < raster.subpasses.size(); ++subpass) {
                        if (subpass)
                            fn.vkCmdNextSubpass2(command, &begin, &end);
                        if (fragments.empty())
                            EncodeVulkanDraws(fn, command, raster.extent, raster.subpasses[subpass]);
                        else if (raster.fragments[subpass].count)
                            fn.vkCmdExecuteCommands(command, raster.fragments[subpass].count, fragments.data() + raster.fragments[subpass].first);
                    }
                    fn.vkCmdEndRenderPass2(command, &end);
                }
                finalBarriers.Encode(fn, command);
            }
            void EncodeFragment(const VulkanDeviceFunctions &fn,
                                VkCommandBuffer              command,
                                const VulkanRasterPassPlan  &pass,
                                const VulkanEncodeTask      &task) const noexcept override
            {
                const auto &raster  = std::get<RenderPassStep>(steps[pass.nativeScope]);
                const auto  subpass = std::get<VulkanRenderPassInheritance>(pass.inheritance).subpass;
                EncodeVulkanDraws(fn, command, raster.extent,
                                  std::span<const VulkanDraw>(raster.subpasses[subpass]).subspan(task.firstDraw, task.drawCount));
            }
            std::vector<std::variant<VulkanTransferStep, RenderPassStep>> steps;
            VulkanBarriers                                                finalBarriers;
        };
    } // namespace
    VulkanCompileResult VulkanRenderPassCompiler::Compile(std::shared_ptr<const FrozenFrameGraph> graph,
                                                          VulkanDeviceFrameContext               &context,
                                                          const DeviceFrameLease                 &lease) noexcept
    {

        auto analysis = AnalyzeFrameGraph(*graph);
        if (auto *error = std::get_if<FrameError>(&analysis))
            return std::move(*error);
        const auto &semantic = std::get<FrameGraphAnalysis>(analysis);
        if (semantic.usedBuffers.empty() && semantic.usedImages.empty())
            return FrameNoWork{};
        const auto &data = graph->GetData();
        for (auto index : semantic.livePasses)
            if (const auto *raster = std::get_if<FrameRasterPass>(&data.passes[index]); raster && raster->colorLocalRead)
                return FrameError{FrameErrorCode::UNSUPPORTED, "RenderPass input attachment bindings are not implemented"};
        VulkanGraphResources resources(device, data, lease.context.deviceEpoch);
        if (auto error = resources.Resolve(semantic))
            return std::move(*error);
        auto encoder     = std::make_unique<VulkanRenderPassEncoder>();
        auto plan        = std::make_unique<VulkanCompiledFrame>();
        plan->livePasses = semantic.livePasses;
        const auto &fn   = device.GetDeviceFn();
        for (size_t cursor = 0; cursor < semantic.livePasses.size();) {
            const auto  index = semantic.livePasses[cursor++];
            const auto &pass  = data.passes[index];
            if (const auto *copy = std::get_if<BufferCopyPass>(&pass)) {
                auto step = resources.Copy(*copy);
                if (auto *error = std::get_if<FrameError>(&step))
                    return std::move(*error);
                encoder->steps.emplace_back(std::move(std::get<VulkanTransferStep>(step)));
            } else if (const auto *copy = std::get_if<BufferImageCopyPass>(&pass)) {
                auto step = resources.Copy(*copy);
                if (auto *error = std::get_if<FrameError>(&step))
                    return std::move(*error);
                encoder->steps.emplace_back(std::move(std::get<VulkanTransferStep>(step)));
            } else {
                const auto &raster = std::get<FrameRasterPass>(pass);
                const auto  first  = cursor - 1;
                const auto *last   = &raster;
                // Only contiguous uses of the same attachment view can omit intermediate stores/loads.
                while (cursor < semantic.livePasses.size()) {
                    const auto *next = std::get_if<FrameRasterPass>(&data.passes[semantic.livePasses[cursor]]);
                    if (!next || raster.colorResolve || next->colorResolve || (raster.depth && raster.depth->resolve) ||
                        (next->depth && next->depth->resolve) || next->color != raster.color || next->colorView != raster.colorView ||
                        next->load != LoadOp::LOAD || last->store != StoreOp::STORE)
                        break;
                    if (next->depth.has_value() != raster.depth.has_value() ||
                        (next->depth && (next->depth->image != raster.depth->image || next->depth->view != raster.depth->view ||
                                         next->depth->load != LoadOp::LOAD || last->depth->store != StoreOp::STORE)))
                        break;
                    last = next;
                    ++cursor;
                }
                const auto  subpassCount = static_cast<uint32_t>(cursor - first);
                auto       *image        = resources.images[raster.color.index];
                const auto &desc         = image->GetDescriptor();
                if (!desc.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                RenderPassStep step;
                step.objects   = std::make_unique<RenderPassObjects>(device);
                auto colorView = resources.AttachmentView(raster.color, raster.colorView);
                if (auto *error = std::get_if<FrameError>(&colorView))
                    return std::move(*error);
                step.objects->colorView   = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(colorView));
                const auto view           = std::get<ResolvedRasterAttachmentView>(ResolveRasterAttachmentView(raster.colorView, desc));
                step.extent               = {view.extent.width, view.extent.height};
                uint32_t depthSubresource = 0;
                for (uint32_t i = 0; i < 4; ++i)
                    step.clear[0].color.float32[i] = raster.clear.color.float32[i];
                std::array<VkAttachmentDescription2, 4> attachments{{{VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2},
                                                                     {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2},
                                                                     {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2},
                                                                     {VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2}}};

                auto &attachment          = attachments[0];
                attachment.format         = image->GetVkFormat();
                attachment.samples        = FromSampleCount(desc.samples);
                attachment.loadOp         = FromLoadOp(raster.load);
                attachment.storeOp        = FromStoreOp(last->store);
                attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment.initialLayout  = FromImageLayout(resources.GetImageState(raster.color.index, view.subresource));
                attachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                VkAttachmentReference2 color{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
                color.attachment = 0;
                color.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                std::array<VkImageView, 4> views{step.objects->colorView->view};
                VkAttachmentReference2     depthReference{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
                if (raster.depth) {
                    auto *depth = resources.images[raster.depth->image.index];
                    if (!depth->GetDescriptor().usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL))
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                    step.attachmentCount = 2;
                    auto depthView       = resources.AttachmentView(raster.depth->image, raster.depth->view);
                    if (auto *error = std::get_if<FrameError>(&depthView))
                        return std::move(*error);
                    step.objects->depthView    = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(depthView));
                    depthSubresource           = raster.depth->view.layer * depth->GetDescriptor().mipLevels + raster.depth->view.mip;
                    views[1]                   = step.objects->depthView->view;
                    step.clear[1].depthStencil = {raster.depth->clear, 0};
                    auto &description          = attachments[1];
                    description.format         = depth->GetVkFormat();
                    description.samples        = FromSampleCount(depth->GetDescriptor().samples);
                    description.loadOp         = FromLoadOp(raster.depth->load);
                    description.storeOp        = FromStoreOp(last->depth->store);
                    description.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    description.initialLayout  = FromImageLayout(resources.GetImageState(raster.depth->image.index, depthSubresource));
                    description.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthReference.attachment  = 1;
                    depthReference.layout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthReference.aspectMask  = VK_IMAGE_ASPECT_DEPTH_BIT;
                }
                VkAttachmentReference2 resolveReference{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
                uint32_t               resolveSubresource = 0;
                if (raster.colorResolve) {
                    const auto &resolve      = *raster.colorResolve;
                    auto        resolvedView = resources.AttachmentView(resolve.image, resolve.view);
                    if (auto *error = std::get_if<FrameError>(&resolvedView))
                        return std::move(*error);
                    step.objects->resolveView   = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(resolvedView));
                    const auto &resolveDesc     = resources.images[resolve.image.index]->GetDescriptor();
                    resolveSubresource          = resolve.view.layer * resolveDesc.mipLevels + resolve.view.mip;
                    const auto attachmentIndex  = step.attachmentCount++;
                    views[attachmentIndex]      = step.objects->resolveView->view;
                    auto &description           = attachments[attachmentIndex];
                    description.format          = image->GetVkFormat();
                    description.samples         = VK_SAMPLE_COUNT_1_BIT;
                    description.loadOp          = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    description.storeOp         = VK_ATTACHMENT_STORE_OP_STORE;
                    description.stencilLoadOp   = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    description.stencilStoreOp  = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    description.initialLayout   = FromImageLayout(resources.GetImageState(resolve.image.index, resolveSubresource));
                    description.finalLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    resolveReference.attachment = attachmentIndex;
                    resolveReference.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                    resolveReference.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                }
                VkAttachmentReference2                  depthResolveReference{VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2};
                VkSubpassDescriptionDepthStencilResolve depthResolve{VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE};
                uint32_t                                depthResolveSubresource = 0;
                if (raster.depth && raster.depth->resolve) {
                    const auto &resolve      = *raster.depth->resolve;
                    auto        resolvedView = resources.AttachmentView(resolve.image, resolve.view);
                    if (auto *error = std::get_if<FrameError>(&resolvedView))
                        return std::move(*error);
                    step.objects->depthResolveView   = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(resolvedView));
                    const auto &resolveDesc          = resources.images[resolve.image.index]->GetDescriptor();
                    depthResolveSubresource          = resolve.view.layer * resolveDesc.mipLevels + resolve.view.mip;
                    const auto attachmentIndex       = step.attachmentCount++;
                    views[attachmentIndex]           = step.objects->depthResolveView->view;
                    auto &description                = attachments[attachmentIndex];
                    description.format               = VK_FORMAT_D32_SFLOAT;
                    description.samples              = VK_SAMPLE_COUNT_1_BIT;
                    description.loadOp               = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    description.storeOp              = VK_ATTACHMENT_STORE_OP_STORE;
                    description.stencilLoadOp        = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                    description.stencilStoreOp       = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                    description.initialLayout        = FromImageLayout(resources.GetImageState(resolve.image.index, depthResolveSubresource));
                    description.finalLayout          = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthResolveReference.attachment = attachmentIndex;
                    depthResolveReference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depthResolveReference.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                    depthResolve.depthResolveMode    = resolve.mode == DepthResolveMode::MINIMUM ? VK_RESOLVE_MODE_MIN_BIT : VK_RESOLVE_MODE_MAX_BIT;
                    depthResolve.pDepthStencilResolveAttachment = &depthResolveReference;
                }
                std::vector<VkSubpassDescription2> subpasses(subpassCount, {VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2});
                for (auto &subpass : subpasses) {
                    subpass.pNext                   = raster.depth && raster.depth->resolve ? &depthResolve : nullptr;
                    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
                    subpass.colorAttachmentCount    = 1;
                    subpass.pColorAttachments       = &color;
                    subpass.pResolveAttachments     = raster.colorResolve ? &resolveReference : nullptr;
                    subpass.pDepthStencilAttachment = raster.depth ? &depthReference : nullptr;
                }
                std::vector<VkSubpassDependency2> dependencies(subpassCount + 1, {VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2});
                dependencies[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
                dependencies[0].dstSubpass    = 0;
                dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                for (uint32_t i = 1; i < subpassCount; ++i) {
                    auto &dependency        = dependencies[i];
                    dependency.srcSubpass   = i - 1;
                    dependency.dstSubpass   = i;
                    dependency.srcStageMask = dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    dependency.srcAccessMask = dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                    dependency.dependencyFlags                          = VK_DEPENDENCY_BY_REGION_BIT;
                }
                auto &outgoing         = dependencies.back();
                outgoing.srcSubpass    = subpassCount - 1;
                outgoing.dstSubpass    = VK_SUBPASS_EXTERNAL;
                outgoing.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                outgoing.dstStageMask  = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
                outgoing.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                outgoing.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
                if (raster.depth) {
                    const auto stages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    const auto access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                    for (auto &dependency : dependencies) {
                        if (dependency.srcSubpass != VK_SUBPASS_EXTERNAL) {
                            dependency.srcStageMask |= stages;
                            dependency.srcAccessMask |= access;
                        }
                        if (dependency.dstSubpass != VK_SUBPASS_EXTERNAL) {
                            dependency.dstStageMask |= stages;
                            dependency.dstAccessMask |= access;
                        }
                    }
                }
                VkRenderPassCreateInfo2 create{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2};
                create.attachmentCount = step.attachmentCount;
                create.pAttachments    = attachments.data();
                create.subpassCount    = subpassCount;
                create.pSubpasses      = subpasses.data();
                create.dependencyCount = static_cast<uint32_t>(dependencies.size());
                create.pDependencies   = dependencies.data();
                auto cached            = device.GetRenderPassCache().Acquire(create, lease.context.deviceEpoch);
                if (auto *error = std::get_if<FrameError>(&cached))
                    return std::move(*error);
                step.objects->cachedPass = std::move(std::get<std::shared_ptr<VulkanCachedRenderPass>>(cached));
                step.objects->renderPass = step.objects->cachedPass->handle;
                VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                framebuffer.renderPass      = step.objects->renderPass;
                framebuffer.attachmentCount = step.attachmentCount;
                framebuffer.pAttachments    = views.data();
                framebuffer.width           = step.extent.width;
                framebuffer.height          = step.extent.height;
                framebuffer.layers          = 1;
                auto status                 = fn.vkCreateFramebuffer(device.GetNativeHandle(), &framebuffer, nullptr, &step.objects->framebuffer);
                if (status != VK_SUCCESS)
                    return VulkanFrameError(status, FrameErrorCode::COMPILE_FAILED);
                for (uint32_t i = 0; i < subpassCount; ++i) {
                    const auto  logical = semantic.livePasses[first + i];
                    SubpassInfo scope{step.objects->renderPass, i, step.objects->cachedPass->identity};
                    const auto &rasterPass = std::get<FrameRasterPass>(data.passes[logical]);
                    if (auto error = resources.RasterInputs(rasterPass, step.before))
                        return std::move(*error);
                    auto draws = ResolveVulkanDraws(device, context, lease, resources, rasterPass, step.extent, desc.format, &scope);
                    if (auto *error = std::get_if<FrameError>(&draws))
                        return std::move(*error);
                    step.subpasses.push_back(std::move(std::get<std::vector<VulkanDraw>>(draws)));
                    VulkanFragmentRange range{static_cast<uint32_t>(plan->encodeTasks.size()), 0};
                    for (uint32_t firstDraw = 0; firstDraw < step.subpasses.back().size();) {
                        const auto count =
                            static_cast<uint32_t>(std::min<size_t>(VULKAN_DRAWS_PER_FRAGMENT, step.subpasses.back().size() - firstDraw));
                        plan->encodeTasks.push_back({static_cast<uint32_t>(plan->rasterPasses.size()), firstDraw, count});
                        firstDraw += count;
                        ++range.count;
                    }
                    step.fragments.push_back(range);
                    plan->rasterPasses.push_back({logical, static_cast<uint32_t>(encoder->steps.size()),
                                                  VulkanRenderPassInheritance{scope.renderPass, step.objects->framebuffer, i}});
                }
                resources.SetImageState(raster.color.index, view.subresource, ImageLayout::COLOR_ATTACHMENT);
                if (raster.colorResolve)
                    resources.SetImageState(raster.colorResolve->image.index, resolveSubresource, ImageLayout::COLOR_ATTACHMENT);
                if (raster.depth)
                    resources.SetImageState(raster.depth->image.index, depthSubresource, ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                if (raster.depth && raster.depth->resolve)
                    resources.SetImageState(raster.depth->resolve->image.index, depthResolveSubresource, ImageLayout::DEPTH_STENCIL_ATTACHMENT);
                encoder->steps.emplace_back(std::move(step));
            }
        }
        resources.Finish(semantic, encoder->finalBarriers);
        plan->resources = std::move(resources.bindings);
        plan->graph     = std::move(graph);
        plan->encoder   = std::move(encoder);
        return plan;
    }
} // namespace sky::aurora
