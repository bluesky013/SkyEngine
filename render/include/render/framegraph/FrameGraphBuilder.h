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

        FrameGraphImageAttachment* CreateImageAttachment(const std::string& source, const std::string& name, VkImageAspectFlags flag);

        FrameGraphImageAttachment* CreateImageAttachment(const std::string& source, const std::string& name, const VkImageSubresourceRange& range);

        void ReadAttachment(const std::string& name, const ImageBindFlag& flag);

        void WriteAttachment(const std::string& name, const ImageBindFlag& flag);

        FrameGraphImageAttachment* ReadWriteAttachment(const std::string& name, const std::string newName, const ImageBindFlag& flag);
    private:
        FrameGraphImageAttachment* GetImageAttachment(const std::string& name);

        FrameGraph& graph;
        FrameGraphPass& pass;
    };

}
