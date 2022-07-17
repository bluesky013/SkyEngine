//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <vulkan/ImageView.h>
#include <render/framegraph/FrameGraphNode.h>
#include <render/framegraph/FrameGraphResource.h>

namespace sky {

    class FrameGraphAttachment : public FrameGraphNode {
    public:
        FrameGraphAttachment(const std::string& str) : FrameGraphNode(str) {}
        ~FrameGraphAttachment() = default;
    };

    enum class ImageBindFlag : uint32_t {
        COLOR,
        COLOR_RESOLVE,
        INPUT,
        DEPTH_STENCIL,
        DEPTH_STENCIL_READ,
        SHADER_READ,
        SHADER_WRITE,
    };

    class FrameGraphImageAttachment : public FrameGraphAttachment {
    public:
        FrameGraphImageAttachment(const std::string& str) : FrameGraphAttachment(str) {}
        ~FrameGraphImageAttachment() = default;

        void Execute(drv::CommandBufferPtr commandBuffer) override {}

        void Compile();

    private:
        friend class FrameGraphBuilder;
        friend class FrameGraphGraphicPass;

        ImageBindFlag bindFlag = ImageBindFlag::COLOR;
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        FrameGraphImage* source = nullptr;
        drv::ImageViewPtr imageView;
        VkClearValue clearValue;
        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    };

}
