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
        FrameGraphAttachment() = default;
        ~FrameGraphAttachment() = default;
    };

    class FrameGraphImageAttachment : public FrameGraphAttachment {
    public:
        FrameGraphImageAttachment() = default;
        ~FrameGraphImageAttachment() = default;

        struct Usage {
            VkPipelineStageFlagBits stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkAccessFlagBits accessFlagBits = VK_ACCESS_NONE_KHR;
            VkImageLayout imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageSubresourceRange range = {
                VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
            };
        };

    private:
        friend class FrameGraphBuilder;
        Usage usage;
        FrameGraphImage* source = nullptr;
        drv::ImageViewPtr imageView;
    };

}
