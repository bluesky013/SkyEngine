//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphTemplate.h>

namespace sky {

    void RenderGraphTemplate::RegisterImage(const std::string& viewTag, const std::string& name)
    {
        auto image = std::make_shared<GraphImage>();
        auto& list = viewGroups[viewTag];
        list.emplace_back(image);
        images.emplace(name, image);
    }

    void RenderGraphTemplate::RegisterAttachment(const std::string& image, const std::string& name)
    {
        auto view = std::make_shared<GraphAttachment>(*images[image]);
        attachments.emplace(name, view);
    }
}