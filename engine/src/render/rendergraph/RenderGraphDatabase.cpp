//
// Created by Zach Lee on 2022/1/2.
//

#include <engine/render/rendergraph/RenderGraphDatabase.h>

namespace sky {

    RGImagePtr RenderGraphDatabase::GetOrCreateImage(std::string key, const drv::Image::Descriptor& des)
    {
        auto iter = cachedImages.find(key);
        if (iter != cachedImages.end()) {
            if (*iter->second == des) {
                return iter->second;
            }
        }
        auto image = std::make_shared<GraphImage>(key);
        if (!image->Init(des)) {
            return nullptr;
        }
        cachedImages.emplace(key, image);
        return image;
    }

}