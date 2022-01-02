//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include <engine/render/rendergraph/RenderGraphResource.h>
#include <unordered_map>
#include <list>

namespace sky {

    class RenderGraphDatabase {
    public:
        RenderGraphDatabase() = default;
        ~RenderGraphDatabase() = default;

        GraphImage* GetOrCreateImage(std::string key, const drv::Image::Descriptor& des);

        GraphAttachment* GetOrCreateAttachment(std::string source, std::string key, const drv::ImageView::Descriptor& des);
    private:
        std::unordered_map<std::string, RGImagePtr> cachedImages;
        std::unordered_map<std::string, RGAttachmentPtr> cachedAttachments;
        std::unordered_map<GraphImage*, std::list<GraphAttachment*>> attachmentsMap;
    };

}