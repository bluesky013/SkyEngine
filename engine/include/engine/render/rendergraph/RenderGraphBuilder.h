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

        RGImagePtr CreateImage(const std::string& str, const drv::Image::Descriptor& desc);

        RGImagePtr ReadImage(const std::string& str);

        bool Read(const std::string& str, const drv::ImageView::Descriptor& desc);

        RGAttachmentPtr Write(const std::string& str, const drv::ImageView::Descriptor& desc);

        void SideEffect();
    private:
        RenderGraph& renderGraph;
        RenderGraphPassBase& pass;
    };

}
