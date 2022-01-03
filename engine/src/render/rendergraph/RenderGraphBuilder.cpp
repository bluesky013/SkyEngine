//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <core/logger/Logger.h>

static const char* TAG = "RenderGraphBuilder";

namespace sky {

    RGImagePtr RenderGraphBuilder::CreateImage(const std::string& str,
        const drv::Image::Descriptor& desc)
    {
        auto res = renderGraph.database.GetOrCreateImage(str, desc);
        renderGraph.resources.emplace(str, res);
        return res;
    }

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

    RGImagePtr RenderGraphBuilder::ReadImage(const std::string& str)
    {
        auto iter = renderGraph.resources.find(str);
        if (iter == renderGraph.resources.end()) {
            return nullptr;
        }
        RenderGraphNode* from = iter->second.get();
        RenderGraphNode* to = &pass;
        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
        auto& vector = renderGraph.incomingEdgeMap[to];
        vector.emplace_back(from);
        return std::static_pointer_cast<GraphImage>(iter->second);
    }

    bool RenderGraphBuilder::Read(const std::string& str, const drv::ImageView::Descriptor& desc)
    {
        auto iter = renderGraph.resources.find(str);
        if (iter == renderGraph.resources.end()) {
            return false;
        }
        RenderGraphNode* from = iter->second.get();
        RenderGraphNode* to = &pass;
        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
        auto& vector = renderGraph.incomingEdgeMap[to];
        vector.emplace_back(from);
        return true;
    }

    RGAttachmentPtr RenderGraphBuilder::Write(const std::string& str, const drv::ImageView::Descriptor& desc)
    {
        auto iter = renderGraph.resources.find(str);
        if (iter == renderGraph.resources.end()) {
            return nullptr;
        }

        auto* image = static_cast<GraphImage*>(iter->second.get());
        auto attachment = std::make_shared<GraphAttachment>(str, image->GetImage());
        if (!attachment->Init(desc)) {
            return nullptr;
        }
        renderGraph.cachedAttachments.emplace_back(attachment);

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