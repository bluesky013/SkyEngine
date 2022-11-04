//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <render/framegraph/FrameGraphNode.h>
#include <render/framegraph/FrameGraphResource.h>
#include <vulkan/ImageView.h>

namespace sky {

    class FrameGraphAttachment : public FrameGraphNode {
    public:
        FrameGraphAttachment(const std::string &str) : FrameGraphNode(str)
        {
        }
        ~FrameGraphAttachment() = default;
    };

    enum class ImageBindFlag : uint32_t {
        UNDEFINED = 0,
        COLOR,
        COLOR_RESOLVE,
        INPUT,
        DEPTH_STENCIL,
        DEPTH_STENCIL_READ,
        SHADER_READ,
        SHADER_WRITE,
        PRESENT
    };

    class FrameGraphImageAttachment : public FrameGraphAttachment {
    public:
        FrameGraphImageAttachment(const std::string &str) : FrameGraphAttachment(str)
        {
        }
        ~FrameGraphImageAttachment() = default;

        void Execute(const vk::CommandBufferPtr &commandBuffer) override
        {
        }

        void Compile();

        FrameGraphImageAttachment &SetColorOp(VkAttachmentLoadOp, VkAttachmentStoreOp);

        FrameGraphImageAttachment &SetClearValue(VkClearValue clearValue);

    private:
        friend class FrameGraphBuilder;
        friend class FrameGraphGraphicPass;

        ImageBindFlag           initFlag  = ImageBindFlag::UNDEFINED;
        ImageBindFlag           bindFlag  = ImageBindFlag::UNDEFINED;
        ImageBindFlag           finalFlag = ImageBindFlag::UNDEFINED;
        VkImageSubresourceRange range     = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        FrameGraphImage        *source    = nullptr;
        vk::ImageViewPtr       imageView;
        VkClearValue            clearValue;
        VkAttachmentLoadOp      loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp     storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp      stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp     stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    };

    enum class BufferBindFlag : uint32_t { SHADER_READ, SHADER_WRITE, VERTEX_INPUT };

    class FrameGraphBufferAttachment : public FrameGraphAttachment {
    public:
        FrameGraphBufferAttachment(const std::string &str) : FrameGraphAttachment(str)
        {
        }
        ~FrameGraphBufferAttachment() = default;

        void Execute(const vk::CommandBufferPtr &commandBuffer) override;

        void Compile();

    private:
        friend class FrameGraphBuilder;
        FrameGraphBuffer *source    = nullptr;
        BufferBindFlag    bindFlag  = BufferBindFlag::SHADER_READ;
        BufferBindFlag    finalFlag = BufferBindFlag::SHADER_READ;
        vk::Barrier      barrier   = {};
    };

} // namespace sky
