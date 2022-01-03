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

        RGImagePtr GetOrCreateImage(std::string key, const drv::Image::Descriptor& des);

    private:
        std::unordered_map<std::string, RGImagePtr> cachedImages;
    };

}