//
// Created by Zach Lee on 2022/6/2.
//

#pragma once

#include <string>
#include <vulkan/Image.h>
#include <render/framegraph/FrameGraphAttachment.h>

namespace sky {
    class FrameGraph;
    class FrameGraphPass;

    class FrameGraphBuilder {
    public:
        FrameGraphBuilder(FrameGraph& g, FrameGraphPass& p) : graph(g), pass(p) {}
        ~FrameGraphBuilder() = default;

        void ImportImage(const std::string& name, drv::ImagePtr);

        void CreateImage(const std::string& name, const drv::Image::Descriptor& imageDesc);

        void ReadImage(const std::string& resKey, const std::string& name, const FrameGraphImageAttachment::Usage& usage, const VkImageSubresourceRange& range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        void WriteImage(const std::string& resKey, const std::string& name, const FrameGraphImageAttachment::Usage& usage, const VkImageSubresourceRange& range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

        void ReadAttachment(const std::string& name, const FrameGraphImageAttachment::Usage& usage);

        void WriteAttachment(const std::string& name, const FrameGraphImageAttachment::Usage& usage);

        void ReadWriteAttachment(const std::string& name, const std::string newName, const FrameGraphImageAttachment::Usage& usage);
    private:
        FrameGraph& graph;
        FrameGraphPass& pass;
    };

}
