//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/RenderPass.h"
#include "core/hash/Crc32.h"
#include "core/hash/Hash.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
#include "vulkan/Conversion.h"
static const char *TAG = "Vulkan";

namespace sky::vk {
    RenderPass::RenderPass(Device &dev) : DevObject(dev), pass(VK_NULL_HANDLE)
    {
    }

    bool RenderPass::Init(const Descriptor &des)
    {
        InitInputMap(des);
        std::vector<std::vector<VkAttachmentReference2>> subpassReferences;
        std::vector<VkSubpassDescriptionDepthStencilResolve> depthStencilResolves;
        depthStencilResolves.reserve(des.subPasses.size());

        attachments.reserve(des.attachments.size());
        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(des.attachments.data()),
                                       static_cast<uint32_t>(des.attachments.size() * sizeof(Attachment))));
        for (const auto &attachment : des.attachments) {
            attachments.emplace_back(VkAttachmentDescription2{});
            auto &vkAt          = attachments.back();
            vkAt.sType          = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
            vkAt.format         = FromRHI(attachment.format);
            vkAt.samples        = FromRHI(attachment.sample);
            vkAt.loadOp         = FromRHI(attachment.load);
            vkAt.storeOp        = FromRHI(attachment.store);
            vkAt.stencilLoadOp  = FromRHI(attachment.stencilLoad);
            vkAt.stencilStoreOp = FromRHI(attachment.stencilStore);
            vkAt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vkAt.finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            samplerCount = std::max(attachment.sample, samplerCount);
        }

        for (const auto &dep : des.dependencies) {
            const auto &srcInfo = device.GetAccessInfo(dep.srcAccess);
            const auto &dstInfo = device.GetAccessInfo(dep.dstAccess);

            dependencies.emplace_back(VkSubpassDependency2{});
            auto &vkDep           = dependencies.back();
            vkDep.sType           = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
            vkDep.srcSubpass      = dep.src;
            vkDep.dstSubpass      = dep.dst;
            vkDep.srcStageMask    = srcInfo.pipelineStages;
            vkDep.dstStageMask    = dstInfo.pipelineStages;
            vkDep.srcAccessMask   = srcInfo.accessFlags;
            vkDep.dstAccessMask   = dstInfo.accessFlags;
            vkDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        }

        auto refFn = [this](std::vector<VkAttachmentReference2>& references, const AttachmentRef &ref) {
            const auto &accessInfo = device.GetAccessInfo(ref.access);

            references.emplace_back(VkAttachmentReference2{});
            auto &vkRef = references.back();
            vkRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
            vkRef.attachment = ref.index;
            vkRef.layout = accessInfo.imageLayout;
            vkRef.aspectMask = FromRHI(ref.mask);

            auto &attachment = attachments[ref.index];
            if (attachment.finalLayout == VK_IMAGE_LAYOUT_UNDEFINED) {
                attachment.initialLayout = vkRef.layout;
            }
            attachment.finalLayout = vkRef.layout;
        };

        auto mvSupported = device.GetFeatures().multiView;

        subPasses.reserve(des.subPasses.size());
        for (const auto &subPass : des.subPasses) {
            subpassReferences.emplace_back();
            subPasses.emplace_back(VkSubpassDescription2{});

            auto &references = subpassReferences.back();
            auto &vkSub = subPasses.back();
            vkSub.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
            auto colorOffset = static_cast<uint32_t>(references.size());
            for (const auto &ref : subPass.colors) {
                refFn(references, ref);
            }
            vkSub.colorAttachmentCount = static_cast<uint32_t>(subPass.colors.size());

            auto resolveOffset = static_cast<uint32_t>(references.size());
            for (const auto &ref : subPass.resolves) {
                refFn(references, ref);
            }

            auto inputsOffset = static_cast<uint32_t>(references.size());
            for (const auto &ref : subPass.inputs) {
                refFn(references, ref);
            }
            vkSub.inputAttachmentCount = static_cast<uint32_t>(subPass.inputs.size());

            vkSub.preserveAttachmentCount = static_cast<uint32_t>(subPass.preserves.size());

            auto depthStencilOffset = static_cast<uint32_t>(references.size());
            if (subPass.depthStencil.index != ~(0U)) {
                refFn(references, subPass.depthStencil);
            }

            auto dsResolveOffset = static_cast<uint32_t>(references.size());
            if (subPass.dsResolve.index != ~(0U)) {
                refFn(references, subPass.dsResolve);

                depthStencilResolves.emplace_back();
                auto &dsResolve = depthStencilResolves.back();
                dsResolve.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE_KHR;
                dsResolve.pNext = nullptr;
                dsResolve.depthResolveMode   = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                dsResolve.stencilResolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
                dsResolve.pDepthStencilResolveAttachment = &references[dsResolveOffset];
                vkSub.pNext = &dsResolve;
            }

            vkSub.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vkSub.viewMask                = mvSupported ? subPass.viewMask : 0;
            vkSub.pInputAttachments       = subPass.inputs.empty() ? nullptr : &references[inputsOffset];
            vkSub.pColorAttachments       = subPass.colors.empty() ? nullptr : &references[colorOffset];
            vkSub.pResolveAttachments     = subPass.resolves.empty() ? nullptr : &references[resolveOffset];
            vkSub.pPreserveAttachments    = subPass.preserves.data();
            vkSub.pDepthStencilAttachment = subPass.depthStencil.index == ~(0U) ? nullptr : &references[depthStencilOffset];

            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.colors.data()),
                                           static_cast<uint32_t>(subPass.colors.size() * sizeof(AttachmentRef))));
            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.resolves.data()),
                                           static_cast<uint32_t>(subPass.resolves.size() * sizeof(AttachmentRef))));
            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.inputs.data()),
                                           static_cast<uint32_t>(subPass.inputs.size() * sizeof(AttachmentRef))));
            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.preserves.data()),
                                           static_cast<uint32_t>(subPass.preserves.size() * sizeof(uint32_t))));
            HashCombine32(hash, Crc32::Cal(subPass.depthStencil));
            HashCombine32(hash, Crc32::Cal(subPass.viewMask));
        }

        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(des.dependencies.data()),
                                       static_cast<uint32_t>(des.dependencies.size() * sizeof(Dependency))));

        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(des.correlatedViewMasks.data()),
                                       static_cast<uint32_t>(des.correlatedViewMasks.size() * sizeof(uint32_t))));

        VkRenderPassCreateInfo2 passInfo = {};
        passInfo.sType                   = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        passInfo.attachmentCount         = static_cast<uint32_t>(attachments.size());
        passInfo.pAttachments            = attachments.data();
        passInfo.subpassCount            = static_cast<uint32_t>(subPasses.size());
        passInfo.pSubpasses              = subPasses.data();
        passInfo.dependencyCount         = static_cast<uint32_t>(dependencies.size());
        passInfo.pDependencies           = dependencies.data();
        passInfo.correlatedViewMaskCount = static_cast<uint32_t>(des.correlatedViewMasks.size());
        passInfo.pCorrelatedViewMasks    = des.correlatedViewMasks.data();

        pass = device.GetRenderPass(hash, &passInfo);
        return pass != VK_NULL_HANDLE;
    }
} // namespace sky::vk
