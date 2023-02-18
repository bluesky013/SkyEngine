//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include "rhi/RenderPass.h"
#include "vulkan/Basic.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <vector>

namespace sky::vk {

    class Device;

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        ~RenderPass();

        struct SubPass {
            std::vector<VkAttachmentReference> colors;
            std::vector<VkAttachmentReference> resolves;
            std::vector<VkAttachmentReference> inputs;
            VkAttachmentReference              depthStencil;
        };

        struct VkDescriptor {
            std::vector<VkAttachmentDescription> attachments;
            std::vector<SubPass>                 subPasses;
            std::vector<VkSubpassDependency>     dependencies;
        };

        VkRenderPass GetNativeHandle() const;

        uint32_t GetHash() const;

    private:
        friend class Device;
        RenderPass(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkRenderPass pass;
        uint32_t     hash;
    };

    using RenderPassPtr = std::shared_ptr<RenderPass>;

    struct RenderPassFactory {
    public:
        RenderPassFactory()  = default;
        ~RenderPassFactory() = default;

        class SubImpl;
        class AttachmentImpl;
        class DependencyImpl;

        class Impl {
        public:
            Impl(RenderPass::VkDescriptor &des) : descriptor(des)
            {
            }
            ~Impl() = default;

            SubImpl AddSubPass();

            DependencyImpl AddDependency();

            RenderPassPtr Create(Device &device);

        protected:
            RenderPass::VkDescriptor &descriptor;
        };

        class DependencyImpl : public Impl {
        public:
            DependencyImpl(RenderPass::VkDescriptor &des, uint32_t dep);
            ~DependencyImpl() = default;

            DependencyImpl &SetLinkage(uint32_t src, uint32_t dst);
            DependencyImpl &SetBarrier(const Barrier &);
            DependencyImpl &SetFlags(VkDependencyFlags flags);

        protected:
            uint32_t dependency;
        };

        class SubImpl : public Impl {
        public:
            SubImpl(RenderPass::VkDescriptor &des, uint32_t sub);
            ~SubImpl() = default;

            AttachmentImpl AddColor();
            AttachmentImpl AddResolve();
            AttachmentImpl AddInput();
            AttachmentImpl AddDepthStencil();

        protected:
            uint32_t subPass;
        };

        class AttachmentImpl : public SubImpl {
        public:
            AttachmentImpl(uint32_t index, RenderPass::VkDescriptor &des, uint32_t sub);
            ~AttachmentImpl() = default;

            AttachmentImpl &Format(VkFormat);
            AttachmentImpl &Samples(VkSampleCountFlagBits);
            AttachmentImpl &ColorOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store);
            AttachmentImpl &StencilOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store);
            AttachmentImpl &Layout(VkImageLayout initial, VkImageLayout final);

        private:
            uint32_t attachment;
        };

        Impl operator()();

    private:
        RenderPass::VkDescriptor descriptor;
    };

} // namespace sky::vk
