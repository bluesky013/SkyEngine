//
// Created by Zach Lee on 2021/12/26.
//

#pragma once
#include <string>
#include <unordered_map>
#include <render/rendergraph/RenderGraphResource.h>
#include <render/rendergraph/RenderGraphPass.h>

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

        void ImportImage(const std::string& str, drv::ImagePtr image);

        RGAttachmentPtr WriteImage(const std::string& str, const drv::ImageView::Descriptor& viewDesc,
            ImageBindingFlag binding, const AttachmentDesc& attachmentDesc);

        RGTexturePtr ReadImage(const std::string& str, const drv::ImageView::Descriptor& desc,
            ImageBindingFlag binding);

//        RGImagePtr CreateImage(const std::string& str, const drv::Image::Descriptor& desc);

        void SideEffect();
    private:
        RenderGraph& renderGraph;
        RenderGraphPassBase& pass;
    };

}
