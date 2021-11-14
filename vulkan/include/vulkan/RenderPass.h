//
// Created by Zach Lee on 2021/12/9.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/Basic.h"
#include <vector>

namespace sky::drv {

    class Device;

    class RenderPass : public DevObject {
    public:
        ~RenderPass();

        struct SubPass {
            std::vector<VkAttachmentReference> colors;
            std::vector<VkAttachmentReference> resolves;
            std::vector<VkAttachmentReference> inputs;
            VkAttachmentReference depthStencil;
        };

        struct Descriptor {
            std::vector<VkAttachmentDescription> attachments;
            std::vector<SubPass> subPasses;
            std::vector<VkSubpassDependency> dependencies;
        };

        bool Init(const Descriptor&);

        VkRenderPass GetNativeHandle() const;

    private:
        friend class Device;
        RenderPass(Device&);

        VkRenderPass pass;
    };

    struct RenderPassFactory {
    public:
        RenderPassFactory() = default;
        ~RenderPassFactory() = default;

        class SubImpl;
        class AttachmentImpl;
        class DependencyImpl;

        class Impl {
        public:
            Impl(RenderPass::Descriptor& des) : descriptor(des) {}
            ~Impl() = default;

            SubImpl AddSubPass();

            DependencyImpl AddDependency();

            RenderPass* Create(Device& device);

        protected:
            RenderPass::Descriptor& descriptor;
        };

        class DependencyImpl : public Impl {
        public:
            DependencyImpl(RenderPass::Descriptor& des, uint32_t dep);
            ~DependencyImpl() = default;

            DependencyImpl& SetLinkage(uint32_t src, uint32_t dst);
            DependencyImpl& SetBarrier(const Barrier&);
            DependencyImpl& SetFlags(VkDependencyFlags flags);
        protected:
            uint32_t dependency;
        };

        class SubImpl : public Impl {
        public:
            SubImpl(RenderPass::Descriptor& des, uint32_t sub)
                : Impl(des), subPass(sub) {}
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
            AttachmentImpl(uint32_t index, RenderPass::Descriptor& des, uint32_t sub);
            ~AttachmentImpl() = default;

            AttachmentImpl& Format(VkFormat);
            AttachmentImpl& Samples(VkSampleCountFlagBits);
            AttachmentImpl& ColorOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store);
            AttachmentImpl& StencilOp(VkAttachmentLoadOp load, VkAttachmentStoreOp store);
            AttachmentImpl& Layout(VkImageLayout initial, VkImageLayout final);

        private:
            uint32_t attachment;
        };

        Impl operator()();

    private:
        RenderPass::Descriptor descriptor;
    };

}
