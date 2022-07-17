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
        FrameGraphImage* fgImage = graph.AddImage<FrameGraphImage>(name);
        fgImage->image = image;
    }

    void FrameGraphBuilder::CreateImage(const std::string& name, const drv::Image::Descriptor& imageDesc)
    {
        auto device = DriverManager::Get()->GetDevice();
        FrameGraphImage* fgImage = graph.AddImage<FrameGraphImage>(name);
        fgImage->image = device->CreateDeviceObject<drv::Image>(imageDesc);
    }

    void FrameGraphBuilder::CreateImageAttachment(const std::string& source, const std::string& name, VkImageAspectFlags flag)
    {
        auto iter = graph.images.find(source);
        if (iter == graph.images.end()) {
            return;
        }
        auto& info = iter->second->image->GetImageInfo();

        auto attachment = graph.AddNode<FrameGraphImageAttachment>(name);
        attachment->range.aspectMask = flag;
        attachment->range.baseArrayLayer = 0;
        attachment->range.baseMipLevel = 0;
        attachment->range.layerCount = info.arrayLayers;
        attachment->range.levelCount = info.mipLevels;
        attachment->source = iter->second.get();
    }

    void FrameGraphBuilder::CreateImageAttachment(const std::string& source, const std::string& name, const VkImageSubresourceRange& range)
    {
        auto iter = graph.images.find(source);
        if (iter == graph.images.end()) {
            return;
        }

        auto attachment = graph.AddNode<FrameGraphImageAttachment>(name);
        attachment->range = range;
        attachment->source = iter->second.get();
    }

    FrameGraphImageAttachment* FrameGraphBuilder::GetImageAttachment(const std::string& name)
    {
        auto iter = graph.nodeTable.find(name);
        if (iter == graph.nodeTable.end()) {
            return nullptr;
        }
        return dynamic_cast<FrameGraphImageAttachment*>(iter->second);
    }

    void FrameGraphBuilder::ReadAttachment(const std::string& name, const ImageBindFlag& flag)
    {
        auto attachment = GetImageAttachment(name);
        if (attachment == nullptr) {
            return;
        }

        attachment->bindFlag = flag;
        pass.UseImageAttachment(attachment);
        graph.AddEdge(attachment, &pass);
    }

    void FrameGraphBuilder::WriteAttachment(const std::string& name, const ImageBindFlag& flag)
    {
        auto attachment = GetImageAttachment(name);
        if (attachment == nullptr) {
            return;
        }

        attachment->bindFlag = flag;
        pass.UseImageAttachment(attachment);
        graph.AddEdge(&pass, attachment);
    }

    void FrameGraphBuilder::ReadWriteAttachment(const std::string& name, const std::string newName, const ImageBindFlag& flag)
    {
        auto oldAttachment = GetImageAttachment(name);
        if (oldAttachment == nullptr) {
            return;
        }

        auto newAttachment = graph.AddNode<FrameGraphImageAttachment>(newName);
        newAttachment->bindFlag = flag;
        newAttachment->range = oldAttachment->range;
        newAttachment->source = oldAttachment->source;

        pass.UseImageAttachment(newAttachment);

        graph.AddEdge(oldAttachment, &pass);
        graph.AddEdge(&pass, newAttachment);
    }

}