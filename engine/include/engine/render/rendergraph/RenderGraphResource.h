//
// Created by Zach Lee on 2021/12/22.
//

#pragma once

#include <vulkan/Image.h>
#include <vulkan/RenderPass.h>
#include <vulkan/FrameBuffer.h>
#include <vulkan/Swapchain.h>
#include <vulkan/Util.h>
#include <vulkan/Sampler.h>
#include <engine/render/rendergraph/RenderGraphNode.h>

namespace sky {

    class RenderGraphPassBase;
    class RenderGraphBuilder;
    class GraphImage;

    struct AttachmentDesc {
        VkAttachmentLoadOp  loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkAttachmentLoadOp  stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    };

    struct ImageBindingFlag {
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkPipelineStageFlags stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    };

    class RenderGraphResource : public RenderGraphNode {
    public:
        RenderGraphResource(std::string&& str) : RenderGraphNode(std::forward<std::string>(str)) {}
        ~RenderGraphResource() = default;

    private:
        friend class RenderGraphBuilder;
        RenderGraphPassBase* first = nullptr;
        RenderGraphPassBase* last = nullptr;
    };
    using RGResourcePtr = std::shared_ptr<RenderGraphResource>;

    class GraphSubImage {
    public:
        GraphSubImage(const drv::ImageView::Descriptor& desc) : descriptor(desc) {}
        ~GraphSubImage() = default;

        VkImageAspectFlags GetAspectFlag() const
        {
            return descriptor.subResourceRange.aspectMask;
        }

        const ImageBindingFlag& GetBinding() const
        {
            return binding;
        }

        void SetBinding(const ImageBindingFlag& flags)
        {
            binding = flags;
        }

        VkFormat GetFormat() const
        {
            return descriptor.format;
        }

        drv::ImageViewPtr GetImageView() const
        {
            return view;
        }

    private:
        friend class GraphImage;
        drv::ImageViewPtr view;
        drv::ImageView::Descriptor descriptor;
        ImageBindingFlag binding;
    };
    using RGSubImagePtr = std::shared_ptr<GraphSubImage>;

    class GraphImage : public RenderGraphResource {
    public:
        GraphImage(std::string str)
            : RenderGraphResource(std::move(str))
        {
        }

        ~GraphImage()
        {
        }

        void SetImage(drv::ImagePtr image);

        drv::ImagePtr GetImage() const;

        RGSubImagePtr GetOrCreateSubImage(const drv::ImageView::Descriptor& desc);

        void BuildResource();
    private:
        drv::ImagePtr image;
        std::unordered_map<uint32_t, RGSubImagePtr> subImages;
    };
    using RGImagePtr = std::shared_ptr<GraphImage>;

    class GraphTexture {
    public:
        GraphTexture(RGSubImagePtr sub, const ImageBindingFlag& last, const ImageBindingFlag& current)
            : subImage(sub)
            , lastBinding(last)
            , currentBinding(current)
        {
        }

        void SetSampler(drv::SamplerPtr spl)
        {
            sampler = spl;
        }

    private:
        friend class RenderGraphBuilder;
        RGSubImagePtr subImage;
        ImageBindingFlag lastBinding;
        ImageBindingFlag currentBinding;
        drv::SamplerPtr sampler;
    };
    using RGTexturePtr = std::shared_ptr<GraphTexture>;

    class GraphAttachment {
    public:
        GraphAttachment(RGSubImagePtr sub, const AttachmentDesc& attachment,
            const ImageBindingFlag& last, const ImageBindingFlag& current)
            : subImage(sub)
            , attachmentDesc(attachment)
            , lastBinding(last)
            , currentBinding(current)
        {
            auto mask = sub->GetAspectFlag();
            if ((mask & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
                clearValue = drv::MakeClearColor(0.f, 0.f, 0.f, 0.f);
            } else {
                clearValue = drv::MakeClearDepthStencil(1.0f, 0);
            }
        }

        ~GraphAttachment() = default;

        void SetClearColor(const VkClearValue& clear)
        {
            clearValue = clear;
        }

        RGSubImagePtr GetSubImage() const
        {
            return subImage;
        }

        const ImageBindingFlag& GetLastBinding() const
        {
            return lastBinding;
        }

        const ImageBindingFlag& GetCurrentBinding() const
        {
            return currentBinding;
        }

        const AttachmentDesc& GetAttachmentDesc() const
        {
            return attachmentDesc;
        }

        drv::ImageViewPtr GetImageView() const
        {
            return subImage->GetImageView();
        }

        const VkClearValue& GetClearValue() const
        {
            return clearValue;
        }

    private:
        friend class RenderGraphBuilder;
        RGSubImagePtr subImage;
        AttachmentDesc attachmentDesc;
        VkClearValue clearValue;
        ImageBindingFlag lastBinding;
        ImageBindingFlag currentBinding;
    };
    using RGAttachmentPtr = std::shared_ptr<GraphAttachment>;
}