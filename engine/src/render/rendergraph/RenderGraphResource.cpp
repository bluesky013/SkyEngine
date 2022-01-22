//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphResource.h>
#include <engine/render/DriverManager.h>
#include <vulkan/Util.h>
#include <core/hash/Crc32.h>

namespace sky {

    void GraphImage::SetImage(drv::ImagePtr img)
    {
        image = img;
    }

    drv::ImagePtr GraphImage::GetImage() const
    {
        return image;
    }

    RGSubImagePtr GraphImage::GetOrCreateSubImage(const drv::ImageView::Descriptor& desc)
    {
        uint32_t hash = Crc32::Cal(desc);
        auto iter = subImages.find(hash);
        if (iter != subImages.end()) {
            return iter->second;
        }
        auto sub = std::make_shared<GraphSubImage>(desc);
        subImages.emplace(hash, sub);
        return sub;
    }

    void GraphImage::BuildResource()
    {
        for (auto& sub : subImages) {
            sub.second->view = image->CreateImageView(sub.second->descriptor);
        }
    }

//    bool GraphAttachment::Init(const drv::ImageView::Descriptor& des)
//    {
//        descriptor = des;
//        return true;
//    }
//
//    bool GraphAttachment::Compile()
//    {
//        auto res = image->CreateImageView(descriptor);
//        if (!res) {
//            return false;
//        }
//        view = res;
//        return true;
//    }
//
//    bool GraphAttachment::operator==(const drv::ImageView::Descriptor& des)
//    {
//        return memcmp(&descriptor, &des, sizeof(drv::ImageView::Descriptor)) == 0;
//    }
//
//    const drv::ImageViewPtr& GraphAttachment::GetImageView() const
//    {
//        return view;
//    }
}