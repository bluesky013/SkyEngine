//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/RenderPipeline.h>

namespace sky {

    bool RenderPipeline::HasViewTag(const std::string& tag) const
    {
        if (!graphTemplate) {
            return false;
        }
        return graphTemplate->HasViewTags(tag);
    }

    void RenderPipeline::Prepare(RenderGraphBuilder& builder, std::list<RenderView*>& list)
    {
        if (graphTemplate) {
            graphTemplate->PreparePipeline(builder, list);
        }
    }
}