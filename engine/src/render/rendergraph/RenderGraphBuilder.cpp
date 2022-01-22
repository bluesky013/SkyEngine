//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <core/logger/Logger.h>

static const char* TAG = "RenderGraphBuilder";

namespace sky {

    void RenderGraphBuilder::ImportImage(const std::string& str, drv::ImagePtr image)
    {
        auto rgImage = std::make_shared<GraphImage>(str);
        rgImage->SetImage(image);
        renderGraph.images.emplace(str, rgImage);
    }

//    RGImagePtr RenderGraphBuilder::CreateImage(const std::string& str,
//        const drv::Image::Descriptor& desc)
//    {
//        auto res = renderGraph.database.GetOrCreateImage(str, desc);
//        renderGraph.resources.emplace(str, res);
//        return res;
//    }

//    GraphAttachment* RenderGraphBuilder::CreateAttachment(const std::string& source, const std::string& str,
//        const drv::ImageView::Descriptor& desc)
//    {
//        auto res = renderGraph.database.GetOrCreateAttachment(source, str, desc);
//        renderGraph.resources.emplace(str, res);
//        return res;
//    }
//
//    GraphFrameBuffer* RenderGraphBuilder::CreateFrameBuffer(const std::string& key, const drv::FrameBuffer::Descriptor& desc)
//    {
//        return renderGraph.database.GetOrCreateFrameBuffer(key, desc);
//    }

//    RGImagePtr RenderGraphBuilder::ReadImage(const std::string& str)
//    {
//        auto iter = renderGraph.resources.find(str);
//        if (iter == renderGraph.resources.end()) {
//            return nullptr;
//        }
//        RenderGraphNode* from = iter->second.get();
//        RenderGraphNode* to = &pass;
//        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
//        auto& vector = renderGraph.incomingEdgeMap[to];
//        vector.emplace_back(from);
//        return std::static_pointer_cast<GraphImage>(iter->second);
//    }
//
//    bool RenderGraphBuilder::Read(const std::string& str, const drv::ImageView::Descriptor& desc)
//    {
//        auto iter = renderGraph.resources.find(str);
//        if (iter == renderGraph.resources.end()) {
//            return false;
//        }
//        RenderGraphNode* from = iter->second.get();
//        RenderGraphNode* to = &pass;
//        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
//        auto& vector = renderGraph.incomingEdgeMap[to];
//        vector.emplace_back(from);
//        return true;
//    }
//
    RGAttachmentPtr RenderGraphBuilder::WriteImage(const std::string& str, const drv::ImageView::Descriptor& viewDesc,
        ImageBindingFlag binding, const AttachmentDesc& attachmentDesc)
    {
        auto iter = renderGraph.images.find(str);
        if (iter == renderGraph.images.end()) {
            return {};
        }

        if (iter->second->first == nullptr) {
            iter->second->first = &pass;
        }
        iter->second->last = &pass;
        auto sub = iter->second->GetOrCreateSubImage(viewDesc);
        auto attachment = std::make_unique<GraphAttachment>(sub, attachmentDesc, sub->GetBinding(), binding);
        sub->SetBinding(binding);

        RenderGraphNode* from = &pass;
        RenderGraphNode* to = iter->second.get();
        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
        auto& vector = renderGraph.incomingEdgeMap[to];
        vector.emplace_back(from);
        return attachment;
    }

    void RenderGraphBuilder::SideEffect()
    {
        pass.SideEffect();
    }

}