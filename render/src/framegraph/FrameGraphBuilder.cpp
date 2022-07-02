//
// Created by Zach Lee on 2022/6/2.
//

#include <render/framegraph/FrameGraphBuilder.h>
#include <render/framegraph/FrameGraphResource.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphAttachment.h>
#include <render/DriverManager.h>

namespace sky {

    void FrameGraphBuilder::ImportImage(const std::string& name, drv::ImagePtr image)
    {
        FrameGraphImage* fgImage = graph.AddResource<FrameGraphImage>(name);
        fgImage->image = image;
    }

    void FrameGraphBuilder::CreateImage(const std::string& name, const drv::Image::Descriptor& imageDesc)
    {
        auto device = DriverManager::Get()->GetDevice();
        FrameGraphImage* fgImage = graph.AddResource<FrameGraphImage>(name);
        fgImage->image = device->CreateDeviceObject<drv::Image>(imageDesc);
    }

    void FrameGraphBuilder::ReadImage(const std::string& resKey, const std::string& name, const FrameGraphImageAttachment::Usage& usage, const VkImageSubresourceRange& range)
    {
        auto attachment = graph.AddNode<FrameGraphImageAttachment>(name);
        attachment->usage = usage;
        attachment->range = range;

        graph.AddEdge(attachment, &pass);
    }

    void FrameGraphBuilder::WriteImage(const std::string& resKey, const std::string& name, const FrameGraphImageAttachment::Usage& usage, const VkImageSubresourceRange& range)
    {
        auto attachment = graph.AddNode<FrameGraphImageAttachment>(name);
        attachment->usage = usage;
        attachment->range = range;

        graph.AddEdge(&pass, attachment);
    }

    void FrameGraphBuilder::ReadAttachment(const std::string& name, const FrameGraphImageAttachment::Usage& usage)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return;
        }

        graph.AddEdge(iter->second, &pass);
    }

    void FrameGraphBuilder::WriteAttachment(const std::string& name, const FrameGraphImageAttachment::Usage& usage)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return;
        }

        graph.AddEdge(&pass, iter->second);
    }

    void FrameGraphBuilder::ReadWriteAttachment(const std::string& name, const std::string newName, const FrameGraphImageAttachment::Usage& usage)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return;
        }

        auto attachment = graph.AddNode<FrameGraphImageAttachment>(newName);
        attachment->usage = usage;

        graph.AddEdge(iter->second, &pass);
        graph.AddEdge(&pass, iter->second);
    }

}