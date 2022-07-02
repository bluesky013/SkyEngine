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

    class FrameGraphImageAttachment : public FrameGraphAttachment {
    public:
        FrameGraphImageAttachment(const std::string& str) : FrameGraphAttachment(str) {}
        ~FrameGraphImageAttachment() = default;

        struct Usage {
            VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        };

    private:
        friend class FrameGraphBuilder;
        Usage usage;
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        FrameGraphImage* source = nullptr;
        drv::ImageViewPtr imageView;
    };

}
