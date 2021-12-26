//
// Created by Zach Lee on 2021/12/26.
//

#pragma once

#include <engine/render/rendergraph/RenderGraphTemplate.h>
#include <string_view>

namespace sky {

    class DeferredRendering : public RenderGraphTemplate {
    public:
        DeferredRendering();
        ~DeferredRendering() = default;

        bool HasViewTags(const std::string&) const override;

        void PreparePipeline(RenderGraphBuilder&, std::list<RenderView*>&) override;

        void SetOutputConfig(const GraphOutput&) override;

    private:
        void BuildResources();

        std::set<std::string> viewTags;
        std::unordered_map<std::string_view, drv::Image::Descriptor> imageDes;
        std::list<std::string_view> resizable;
    };

}