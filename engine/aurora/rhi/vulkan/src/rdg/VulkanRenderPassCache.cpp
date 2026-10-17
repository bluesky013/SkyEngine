#include "VulkanRenderPassCache.h"
#include "VulkanGraphResources.h"
#include <VulkanDevice.h>

namespace sky::aurora {
    VulkanCachedRenderPass::~VulkanCachedRenderPass()
    {
        if (handle)
            device.GetDeviceFn().vkDestroyRenderPass(device.GetNativeHandle(), handle, nullptr);
    }
    std::variant<FrameError, std::shared_ptr<VulkanCachedRenderPass>> VulkanRenderPassCache::Acquire(const VkRenderPassCreateInfo2 &info,
                                                                                                     uint64_t                       epoch)
    {
        if (info.pNext)
            return FrameError{FrameErrorCode::UNSUPPORTED, "RenderPass cache extension is not represented in its key"};
        std::vector<uint64_t> key{epoch, info.flags, info.attachmentCount, info.subpassCount, info.dependencyCount, info.correlatedViewMaskCount};
        auto                  append    = [&](std::initializer_list<uint64_t> values) { key.insert(key.end(), values); };
        auto                  reference = [&](const VkAttachmentReference2 *value) {
            if (!value) {
                key.push_back(UINT64_MAX);
                return;
            }
            append({value->attachment, static_cast<uint64_t>(value->layout), value->aspectMask});
        };
        for (uint32_t i = 0; i < info.attachmentCount; ++i) {
            const auto &a = info.pAttachments[i];
            if (a.pNext)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Attachment extension is not represented in the cache key"};
            append({a.flags, static_cast<uint64_t>(a.format), static_cast<uint64_t>(a.samples), static_cast<uint64_t>(a.loadOp),
                    static_cast<uint64_t>(a.storeOp), static_cast<uint64_t>(a.stencilLoadOp), static_cast<uint64_t>(a.stencilStoreOp),
                    static_cast<uint64_t>(a.initialLayout), static_cast<uint64_t>(a.finalLayout)});
        }
        for (uint32_t i = 0; i < info.subpassCount; ++i) {
            const auto &s = info.pSubpasses[i];
            append({s.flags, static_cast<uint64_t>(s.pipelineBindPoint), s.viewMask, s.inputAttachmentCount, s.colorAttachmentCount,
                    s.preserveAttachmentCount});
            for (uint32_t j = 0; j < s.inputAttachmentCount; ++j)
                reference(s.pInputAttachments + j);
            for (uint32_t j = 0; j < s.colorAttachmentCount; ++j) {
                reference(s.pColorAttachments + j);
                reference(s.pResolveAttachments ? s.pResolveAttachments + j : nullptr);
            }
            reference(s.pDepthStencilAttachment);
            for (uint32_t j = 0; j < s.preserveAttachmentCount; ++j)
                key.push_back(s.pPreserveAttachments[j]);
            const auto *resolve = static_cast<const VkSubpassDescriptionDepthStencilResolve *>(s.pNext);
            if (resolve && (resolve->sType != VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE || resolve->pNext))
                return FrameError{FrameErrorCode::UNSUPPORTED, "Subpass extension is not represented in the cache key"};
            append(
                {resolve ? static_cast<uint64_t>(resolve->depthResolveMode) : 0, resolve ? static_cast<uint64_t>(resolve->stencilResolveMode) : 0});
            reference(resolve ? resolve->pDepthStencilResolveAttachment : nullptr);
        }
        for (uint32_t i = 0; i < info.dependencyCount; ++i) {
            const auto &d = info.pDependencies[i];
            if (d.pNext)
                return FrameError{FrameErrorCode::UNSUPPORTED, "Dependency extension is not represented in the cache key"};
            append({d.srcSubpass, d.dstSubpass, d.srcStageMask, d.dstStageMask, d.srcAccessMask, d.dstAccessMask, d.dependencyFlags,
                    static_cast<uint64_t>(d.viewOffset)});
        }
        for (uint32_t i = 0; i < info.correlatedViewMaskCount; ++i)
            key.push_back(info.pCorrelatedViewMasks[i]);
        return cache.Acquire(std::move(key), [&]() -> std::variant<FrameError, std::shared_ptr<VulkanCachedRenderPass>> {
            if (nextIdentity == UINT64_MAX)
                return FrameError{FrameErrorCode::COMPILE_FAILED, "RenderPass cache identity exhausted"};
            auto       pass   = std::make_shared<VulkanCachedRenderPass>(device);
            const auto status = device.GetDeviceFn().vkCreateRenderPass2(device.GetNativeHandle(), &info, nullptr, &pass->handle);
            if (status != VK_SUCCESS)
                return VulkanFrameError(status, FrameErrorCode::COMPILE_FAILED);
            pass->identity = nextIdentity++;
            return pass;
        });
    }
} // namespace sky::aurora
