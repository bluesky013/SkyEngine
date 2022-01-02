//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphBuilder.h>
#include <engine/render/rendergraph/RenderGraph.h>
#include <core/logger/Logger.h>

static const char* TAG = "RenderGraphBuilder";

namespace sky {

    GraphImage* RenderGraphBuilder::CreateImage(const std::string& str,
        const drv::Image::Descriptor& desc)
    {
        auto res = renderGraph.database.GetOrCreateImage(str, desc);
        renderGraph.resources.emplace(str, res);
        return res;
    }

    GraphAttachment* RenderGraphBuilder::CreateAttachment(const std::string& source, const std::string& str,
        const drv::ImageView::Descriptor& desc)
    {
        auto res = renderGraph.database.GetOrCreateAttachment(source, str, desc);
        renderGraph.resources.emplace(str, res);
        return res;
    }

    bool RenderGraphBuilder::Read(const std::string& str)
    {
        auto iter = renderGraph.resources.find(str);
        if (iter == renderGraph.resources.end()) {
            return false;
        }
        RenderGraphNode* from = iter->second;
        RenderGraphNode* to = &pass;
        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
        auto& vector = renderGraph.incomingEdgeMap[to];
        vector.emplace_back(from);
        return true;
    }

    bool RenderGraphBuilder::Write(const std::string& str)
    {
        auto iter = renderGraph.resources.find(str);
        if (iter == renderGraph.resources.end()) {
            return false;
        }

        RenderGraphNode* from = &pass;
        RenderGraphNode* to = iter->second;
        renderGraph.edges.emplace_back(RenderGraph::Edge{from, to});
        auto& vector = renderGraph.incomingEdgeMap[to];
        vector.emplace_back(from);
        return true;
    }

}