//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <engine/render/rendergraph/RenderGraphTemplate.h>
#include <list>

namespace sky {

    class RenderView;

    class RenderPipeline  {
    public:
        RenderPipeline() = default;
        ~RenderPipeline() = default;

        template <typename T>
        void BuildTemplate()
        {
            graphTemplate = std::make_unique<T>();
        }

        bool HasViewTag(const std::string& tag) const;

        void Prepare(RenderGraphBuilder& builder, std::list<RenderView*>& list);

    private:
        std::unique_ptr<RenderGraphTemplate> graphTemplate;
    };

}