#include "VulkanFrameCompiler.h"
#include "VulkanGraphResources.h"
#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <algorithm>

namespace sky::aurora {
    namespace {
        struct DynamicRasterStep {
            VulkanBarriers                        before;
            VkRenderingAttachmentInfo             color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            VkRenderingAttachmentInfo             depth{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            VkExtent2D                            extent{};
            std::vector<VulkanDraw>               draws;
            VulkanFragmentRange                   fragments;
            std::shared_ptr<VulkanGraphImageView> colorView, depthView, resolveView, depthResolveView;
        };
        class VulkanDynamicRenderingEncoder final : public VulkanFrameEncoder {
        public:
            void Encode(const VulkanDeviceFunctions &fn, VkCommandBuffer command, std::span<const VkCommandBuffer> fragments) const noexcept override
            {
                for (const auto &step : steps) {
                    if (const auto *copy = std::get_if<VulkanTransferStep>(&step)) {
                        copy->Encode(fn, command);
                        continue;
                    }
                    const auto &raster = std::get<DynamicRasterStep>(step);
                    raster.before.Encode(fn, command);
                    VkRenderingInfo info{VK_STRUCTURE_TYPE_RENDERING_INFO};
                    info.flags                = fragments.empty() ? 0 : VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
                    info.renderArea.extent    = raster.extent;
                    info.layerCount           = 1;
                    info.colorAttachmentCount = 1;
                    info.pColorAttachments    = &raster.color;
                    info.pDepthAttachment     = raster.depth.imageView ? &raster.depth : nullptr;
                    fn.vkCmdBeginRendering(command, &info);
                    if (fragments.empty())
                        EncodeVulkanDraws(fn, command, raster.extent, raster.draws);
                    else if (raster.fragments.count)
                        fn.vkCmdExecuteCommands(command, raster.fragments.count, fragments.data() + raster.fragments.first);
                    fn.vkCmdEndRendering(command);
                }
                finalBarriers.Encode(fn, command);
            }
            void EncodeFragment(const VulkanDeviceFunctions &fn,
                                VkCommandBuffer              command,
                                const VulkanRasterPassPlan  &pass,
                                const VulkanEncodeTask      &task) const noexcept override
            {
                const auto &raster = std::get<DynamicRasterStep>(steps[pass.nativeScope]);
                EncodeVulkanDraws(fn, command, raster.extent, std::span<const VulkanDraw>(raster.draws).subspan(task.firstDraw, task.drawCount));
            }
            std::vector<std::variant<VulkanTransferStep, DynamicRasterStep>> steps;
            VulkanBarriers                                                   finalBarriers;
        };
    } // namespace
    VulkanCompileResult VulkanDynamicRenderingCompiler::Compile(std::shared_ptr<const FrozenFrameGraph> graph,
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
                return FrameError{FrameErrorCode::UNSUPPORTED, "Dynamic rendering local read is not enabled"};
        VulkanGraphResources resources(device, data, lease.context.deviceEpoch);
        if (auto error = resources.Resolve(semantic))
            return std::move(*error);
        auto encoder     = std::make_unique<VulkanDynamicRenderingEncoder>();
        auto plan        = std::make_unique<VulkanCompiledFrame>();
        plan->livePasses = semantic.livePasses;
        for (auto index : semantic.livePasses) {
            const auto &pass = data.passes[index];
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
                auto       *image  = resources.images[raster.color.index];
                const auto &desc   = image->GetDescriptor();
                if (!desc.usage.TestBit(ImageUsageFlagBit::RENDER_TARGET))
                    return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                DynamicRasterStep step;
                if (auto error = resources.RasterInputs(raster, step.before))
                    return std::move(*error);
                auto colorView = resources.AttachmentView(raster.color, raster.colorView);
                if (auto *error = std::get_if<FrameError>(&colorView))
                    return std::move(*error);
                step.colorView  = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(colorView));
                const auto view = std::get<ResolvedRasterAttachmentView>(ResolveRasterAttachmentView(raster.colorView, desc));
                resources.ImageBarrier(raster.color.index, ImageLayout::COLOR_ATTACHMENT, step.before, view.range);
                step.extent            = {view.extent.width, view.extent.height};
                step.color.imageView   = step.colorView->view;
                step.color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                step.color.loadOp      = FromLoadOp(raster.load);
                step.color.storeOp     = FromStoreOp(raster.store);
                for (uint32_t i = 0; i < 4; ++i)
                    step.color.clearValue.color.float32[i] = raster.clear.color.float32[i];
                if (raster.depth) {
                    auto *depth = resources.images[raster.depth->image.index];
                    if (!depth->GetDescriptor().usage.TestBit(ImageUsageFlagBit::DEPTH_STENCIL))
                        return FrameError{FrameErrorCode::INVALID_ARGUMENT};
                    auto depthView = resources.AttachmentView(raster.depth->image, raster.depth->view);
                    if (auto *error = std::get_if<FrameError>(&depthView))
                        return std::move(*error);
                    step.depthView = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(depthView));
                    resources.ImageBarrier(raster.depth->image.index, ImageLayout::DEPTH_STENCIL_ATTACHMENT, step.before,
                                           {raster.depth->view.mip, 1, raster.depth->view.layer, 1});
                    step.depth.imageView               = step.depthView->view;
                    step.depth.imageLayout             = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    step.depth.loadOp                  = FromLoadOp(raster.depth->load);
                    step.depth.storeOp                 = FromStoreOp(raster.depth->store);
                    step.depth.clearValue.depthStencil = {raster.depth->clear, 0};
                    if (raster.depth->resolve) {
                        const auto &resolve      = *raster.depth->resolve;
                        auto        resolvedView = resources.AttachmentView(resolve.image, resolve.view);
                        if (auto *error = std::get_if<FrameError>(&resolvedView))
                            return std::move(*error);
                        step.depthResolveView = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(resolvedView));
                        resources.ImageBarrier(resolve.image.index, ImageLayout::DEPTH_STENCIL_ATTACHMENT, step.before,
                                               {resolve.view.mip, 1, resolve.view.layer, 1});
                        step.depth.resolveMode        = resolve.mode == DepthResolveMode::MINIMUM ? VK_RESOLVE_MODE_MIN_BIT : VK_RESOLVE_MODE_MAX_BIT;
                        step.depth.resolveImageView   = step.depthResolveView->view;
                        step.depth.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    }
                }
                if (raster.colorResolve) {
                    const auto &resolve     = *raster.colorResolve;
                    auto        resolveView = resources.AttachmentView(resolve.image, resolve.view);
                    if (auto *error = std::get_if<FrameError>(&resolveView))
                        return std::move(*error);
                    step.resolveView = std::move(std::get<std::shared_ptr<VulkanGraphImageView>>(resolveView));
                    resources.ImageBarrier(resolve.image.index, ImageLayout::COLOR_ATTACHMENT, step.before,
                                           {resolve.view.mip, 1, resolve.view.layer, 1});
                    step.color.resolveMode        = VK_RESOLVE_MODE_AVERAGE_BIT;
                    step.color.resolveImageView   = step.resolveView->view;
                    step.color.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
                auto draws = ResolveVulkanDraws(device, context, lease, resources, raster, step.extent, desc.format, nullptr);
                if (auto *error = std::get_if<FrameError>(&draws))
                    return std::move(*error);
                step.draws           = std::move(std::get<std::vector<VulkanDraw>>(draws));
                step.fragments.first = static_cast<uint32_t>(plan->encodeTasks.size());
                for (uint32_t first = 0; first < step.draws.size();) {
                    const auto count = static_cast<uint32_t>(std::min<size_t>(VULKAN_DRAWS_PER_FRAGMENT, step.draws.size() - first));
                    plan->encodeTasks.push_back({static_cast<uint32_t>(plan->rasterPasses.size()), first, count});
                    first += count;
                    ++step.fragments.count;
                }
                // Separate scopes keep intervening attachment hazards outside dynamic rendering.
                plan->rasterPasses.push_back({index, static_cast<uint32_t>(encoder->steps.size()),
                                              VulkanRenderingInheritance{image->GetVkFormat(), FromSampleCount(desc.samples), 0,
                                                                         raster.depth ? VK_FORMAT_D32_SFLOAT : VK_FORMAT_UNDEFINED}});
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
