//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphBuilder.h>
#include <render/framegraph/FrameGraphResource.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphAttachment.h>

namespace sky {

    void FrameGraphBuilder::ImportImage(const std::string& name, drv::ImagePtr image)
    {
        FrameGraphImage* fgImage = graph.AddResource<FrameGraphImage>(name);
        fgImage->image = image;

        auto attachment = graph.AddNode<FrameGraphImageAttachment>();
        attachment->source = fgImage;
        graph.nodeTable.emplace(name, attachment);
    }

    void FrameGraphBuilder::ReadImage(const std::string& name, const FrameGraphImageAttachment::Usage& usage)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return;
        }

        graph.AddEdge(iter->second, &pass);
    }

    void FrameGraphBuilder::WriteImage(const std::string& name, const FrameGraphImageAttachment::Usage& usage)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return;
        }

        graph.AddEdge(&pass, iter->second);
    }

}