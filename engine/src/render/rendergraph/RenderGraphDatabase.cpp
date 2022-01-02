//
// Created by Zach Lee on 2022/1/2.
//

#include <engine/render/rendergraph/RenderGraphDatabase.h>

namespace sky {

    GraphImage* RenderGraphDatabase::GetOrCreateImage(std::string key, const drv::Image::Descriptor& des)
    {
        auto iter = cachedImages.find(key);
        if (iter != cachedImages.end()) {
            if (*iter->second == des) {
                return iter->second.get();
            }
            auto mapIt = attachmentsMap.find(iter->second.get());
            if (mapIt != attachmentsMap.end()) {
                for (auto& view : mapIt->second) {
                    cachedAttachments.erase(view->GetName());
                }
                attachmentsMap.erase(mapIt);
            }
        }
        auto image = std::make_unique<GraphImage>(key);
        if (!image->Init(des)) {
            return nullptr;
        }
        auto res = image.get();
        cachedImages.emplace(key, std::move(image));
        return res;
    }

    GraphAttachment* RenderGraphDatabase::GetOrCreateAttachment(std::string source, std::string key,
        const drv::ImageView::Descriptor& des)
    {
        auto view = cachedAttachments.find(key);
        if (view != cachedAttachments.end()) {
            if (*view->second == des) {
                return view->second.get();
            }
        }

        auto iter = cachedImages.find(source);
        if (iter == cachedImages.end()) {
            return nullptr;
        }

        auto attachment = std::make_unique<GraphAttachment>(key, iter->second->GetImage());
        if (!attachment->Init(des)) {
            return nullptr;
        }
        auto res = attachment.get();
        cachedAttachments.emplace(key, std::move(attachment));
        return res;
    }

}