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

        void ReadImage(const std::string& name, const FrameGraphImageAttachment::Usage& usage);

        void WriteImage(const std::string& name, const FrameGraphImageAttachment::Usage& usage);

    private:
        FrameGraph& graph;
        FrameGraphPass& pass;
    };

}
