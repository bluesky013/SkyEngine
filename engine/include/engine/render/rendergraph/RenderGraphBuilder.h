//
// Created by Zach Lee on 2021/12/26.
//

#pragma once
#include <string>
#include <unordered_map>
#include <engine/render/rendergraph/RenderGraphResource.h>
#include <engine/render/rendergraph/RenderGraphPass.h>

namespace sky {

    class RenderGraph;

    class RenderGraphBuilder {
    public:
        RenderGraphBuilder(RenderGraph& rg, RenderGraphPassBase& ps)
            : renderGraph(rg)
            , pass(ps)
        {
        }
        ~RenderGraphBuilder() = default;

        GraphImage* CreateImage(const std::string& str, const drv::Image::Descriptor& desc);

        GraphAttachment* CreateAttachment(const std::string& source, const std::string& str, const drv::ImageView::Descriptor& desc);

        bool Read(const std::string& str);

        bool Write(const std::string& str);

    private:
        RenderGraph& renderGraph;
        RenderGraphPassBase& pass;
    };

}
