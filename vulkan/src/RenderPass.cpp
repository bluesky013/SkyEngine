//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/RenderPass.h"
#include "core/hash/Crc32.h"
#include "core/hash/Hash.h"
#include "core/logger/Logger.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
static const char *TAG = "Vulkan";

namespace sky::vk {

    RenderPass::RenderPass(Device &dev) : DevObject(dev), pass(VK_NULL_HANDLE), hash(0), psoHash(0)
    {
    }

    RenderPass::~RenderPass()
    {
    }

    bool RenderPass::Init(const Descriptor &des)
    {
        hash = 0;
        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(des.attachments.data()),
                                       static_cast<uint32_t>(des.attachments.size() * sizeof(VkAttachmentDescription))));

        std::vector<VkSubpassDescription> subPasses(des.subPasses.size());
        for (uint32_t i = 0; i < subPasses.size(); ++i) {
            auto &vSubPass = subPasses[i];
            auto &subPass  = des.subPasses[i];

            vSubPass.flags                   = 0;
            vSubPass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            vSubPass.inputAttachmentCount    = (uint32_t)subPass.inputs.size();
            vSubPass.pInputAttachments       = subPass.inputs.empty() ? nullptr : subPass.inputs.data();
            vSubPass.colorAttachmentCount    = (uint32_t)subPass.colors.size();
            vSubPass.pColorAttachments       = subPass.colors.empty() ? nullptr : subPass.colors.data();
            vSubPass.pResolveAttachments     = subPass.resolves.empty() ? nullptr : subPass.resolves.data();
            vSubPass.pDepthStencilAttachment = subPass.depthStencil.attachment == -1 ? nullptr : &subPass.depthStencil;

            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.colors.data()),
                                           static_cast<uint32_t>(subPass.colors.size() * sizeof(VkAttachmentReference))));
            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.resolves.data()),
                                           static_cast<uint32_t>(subPass.resolves.size() * sizeof(VkAttachmentReference))));
            HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(subPass.inputs.data()),
                                           static_cast<uint32_t>(subPass.inputs.size() * sizeof(VkAttachmentReference))));
            HashCombine32(hash, Crc32::Cal(subPass.depthStencil));
        }

        HashCombine32(hash, Crc32::Cal(reinterpret_cast<const uint8_t *>(des.dependencies.data()),
                                       static_cast<uint32_t>(des.dependencies.size() * sizeof(VkSubpassDependency))));

        VkRenderPassCreateInfo passInfo = {};
        passInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        passInfo.attachmentCount        = (uint32_t)des.attachments.size();
        passInfo.pAttachments           = des.attachments.data();
        passInfo.subpassCount           = (uint32_t)des.subPasses.size();
        passInfo.pSubpasses             = subPasses.data();
        passInfo.dependencyCount        = (uint32_t)des.dependencies.size();
        passInfo.pDependencies          = des.dependencies.data();

        pass = device.GetRenderPass(hash, &passInfo);
        if (pass == VK_NULL_HANDLE) {
            return false;
        }
        return true;
    }

    VkRenderPass RenderPass::GetNativeHandle() const
    {
        return pass;
    }

    uint32_t RenderPass::GetHash() const
    {
        return hash;
    }

    uint32_t RenderPass::GetPsoHash() const
    {
        return psoHash;
    }

    static void InitAttachmentInfo(VkAttachmentDescription &attachment)
    {
        attachment.flags          = 0;
        attachment.format         = VK_FORMAT_UNDEFINED;
        attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    static void InitSubPass(RenderPass::SubPass &subPass)
    {
        subPass.depthStencil.attachment = -1;
        subPass.depthStencil.layout     = VK_IMAGE_LAYOUT_UNDEFINED;
    }

    static void InitDependency(VkSubpassDependency &dependency)
    {
        dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask    = 0;
        dependency.dstStageMask    = 0;
        dependency.srcAccessMask   = 0;
        dependency.dstAccessMask   = 0;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
    }

    /**
     * Render Pass Compatibility
     */
    void RenderPass::CalculateHashForPSO(const Descriptor &desc)
    {
        psoHash = 0;

        auto calc = [this](const Descriptor &desc, const VkAttachmentReference &ref) {
            auto &attachment = desc.attachments[ref.attachment];
            HashCombine32(psoHash, attachment.format);
            HashCombine32(psoHash, attachment.samples);
        };

        for (auto &subPass : desc.subPasses) {
            for (auto &attachment : subPass.colors) {
                calc(desc, attachment);
            }
            for (auto &attachment : subPass.resolves) {
                calc(desc, attachment);
            }
            for (auto &attachment : subPass.inputs) {
                calc(desc, attachment);
            }
            calc(desc, subPass.depthStencil);
        }
    }

    RenderPassFactory::Impl RenderPassFactory::operator()()
    {
        return RenderPassFactory::Impl(descriptor);
    }

    RenderPassPtr RenderPassFactory::Impl::Create(Device &device)
    {
        return device.CreateDeviceObject<RenderPass>(descriptor);
    }

    RenderPassFactory::SubImpl RenderPassFactory::Impl::AddSubPass()
    {
        descriptor.subPasses.emplace_back(RenderPass::SubPass{});
        InitSubPass(descriptor.subPasses.back());
        return RenderPassFactory::SubImpl(descriptor, (uint32_t)descriptor.subPasses.size() - 1);
    }

    RenderPassFactory::DependencyImpl RenderPassFactory::Impl::AddDependency()
    {
        descriptor.dependencies.emplace_back(VkSubpassDependency{});
        InitDependency(descriptor.dependencies.back());
        return RenderPassFactory::DependencyImpl(descriptor, (uint32_t)descriptor.dependencies.size() - 1);
    }

    RenderPassFactory::DependencyImpl::DependencyImpl(RenderPass::Descriptor &des, uint32_t dep) : Impl(des), dependency(dep)
    {
    }

    RenderPassFactory::DependencyImpl &RenderPassFactory::DependencyImpl::SetLinkage(uint32_t src, uint32_t dst)
    {
        auto &vDep      = descriptor.dependencies[dependency];
        vDep.srcSubpass = src;
        vDep.dstSubpass = dst;
        return *this;
    }

    RenderPassFactory::DependencyImpl &RenderPassFactory::DependencyImpl::SetBarrier(const Barrier &barrier)
    {
        auto &vDep         = descriptor.dependencies[dependency];
        vDep.srcStageMask  = barrier.srcStageMask;
        vDep.dstStageMask  = barrier.dstStageMask;
        vDep.srcAccessMask = barrier.srcAccessMask;
        vDep.dstAccessMask = barrier.dstAccessMask;
        return *this;
    }

    RenderPassFactory::DependencyImpl &RenderPassFactory::DependencyImpl::SetFlags(VkDependencyFlags flags)
    {
        auto &vDep           = descriptor.dependencies[dependency];
        vDep.dependencyFlags = flags;
        return *this;
    }

    RenderPassFactory::SubImpl::SubImpl(RenderPass::Descriptor &des, uint32_t index) : Impl(des), subPass(index)
    {
    }

    RenderPassFactory::AttachmentImpl RenderPassFactory::SubImpl::AddColor()
    {
        auto &sub = descriptor.subPasses[subPass];
        sub.colors.emplace_back(
            VkAttachmentReference{static_cast<uint32_t>(descriptor.attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        descriptor.attachments.emplace_back();
        InitAttachmentInfo(descriptor.attachments.back());
        return RenderPassFactory::AttachmentImpl((uint32_t)descriptor.attachments.size() - 1, descriptor, subPass);
    }

    RenderPassFactory::AttachmentImpl RenderPassFactory::SubImpl::AddResolve()
    {
        auto &sub = descriptor.subPasses[subPass];
        sub.resolves.emplace_back(
            VkAttachmentReference{static_cast<uint32_t>(descriptor.attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        descriptor.attachments.emplace_back();
        InitAttachmentInfo(descriptor.attachments.back());
        return RenderPassFactory::AttachmentImpl((uint32_t)descriptor.attachments.size() - 1, descriptor, subPass);
    }

    RenderPassFactory::AttachmentImpl RenderPassFactory::SubImpl::AddInput()
    {
        auto &sub = descriptor.subPasses[subPass];
        sub.inputs.emplace_back(
            VkAttachmentReference{static_cast<uint32_t>(descriptor.attachments.size()), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        descriptor.attachments.emplace_back();
        InitAttachmentInfo(descriptor.attachments.back());
        return RenderPassFactory::AttachmentImpl((uint32_t)descriptor.attachments.size() - 1, descriptor, subPass);
    }

    RenderPassFactory::AttachmentImpl RenderPassFactory::SubImpl::AddDepthStencil()
    {
        auto &sub                   = descriptor.subPasses[subPass];
        sub.depthStencil.attachment = static_cast<uint32_t>(descriptor.attachments.size());
        sub.depthStencil.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        descriptor.attachments.emplace_back();
        InitAttachmentInfo(descriptor.attachments.back());
        return RenderPassFactory::AttachmentImpl((uint32_t)descriptor.attachments.size() - 1, descriptor, subPass);
    }

    RenderPassFactory::AttachmentImpl::AttachmentImpl(uint32_t index, RenderPass::Descriptor &des, uint32_t sub)
    : SubImpl(des, sub), attachment(index)
    {
    }

    RenderPassFactory::AttachmentImpl &RenderPassFactory::AttachmentImpl::Format(VkFormat format)
    {
        auto &vAttachment  = descriptor.attachments[attachment];
        vAttachment.format = format;
        return *this;
    }

    RenderPassFactory::AttachmentImpl &RenderPassFactory::AttachmentImpl::Samples(VkSampleCountFlagBits samples)
    {
        auto &vAttachment   = descriptor.attachments[attachment];
        vAttachment.samples = samples;
        return *this;
    }

    RenderPassFactory::AttachmentImpl &RenderPassFactory::AttachmentImpl::ColorOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store)
    {
        auto &vAttachment   = descriptor.attachments[attachment];
        vAttachment.loadOp  = load;
        vAttachment.storeOp = store;
        return *this;
    }

    RenderPassFactory::AttachmentImpl &RenderPassFactory::AttachmentImpl::StencilOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store)
    {
        auto &vAttachment          = descriptor.attachments[attachment];
        vAttachment.stencilLoadOp  = load;
        vAttachment.stencilStoreOp = store;
        return *this;
    }

    RenderPassFactory::AttachmentImpl &RenderPassFactory::AttachmentImpl::Layout(VkImageLayout initial, VkImageLayout final)
    {
        auto &vAttachment         = descriptor.attachments[attachment];
        vAttachment.initialLayout = initial;
        vAttachment.finalLayout   = final;
        return *this;
    }

} // namespace sky::vk
