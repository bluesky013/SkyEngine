//
// Created by Zach Lee on 2021/12/26.
//

#include <engine/render/rendergraph/RenderGraphResource.h>
#include <engine/render/DriverManager.h>
#include <vulkan/Util.h>

namespace sky {

    bool GraphImage::Init(const drv::Image::Descriptor& des)
    {
        auto device = DriverManager::Get()->GetDevice();
        auto res = device->CreateDeviceObject<drv::Image>(des);
        if (!res) {
            return false;
        }
        image = res;
        descriptor = des;
        return true;
    }

    bool GraphImage::operator==(const drv::Image::Descriptor& des)
    {
        return memcmp(&descriptor, &des, sizeof(drv::Image::Descriptor)) == 0;
    }

    drv::ImagePtr GraphImage::GetImage() const
    {
        return image;
    }

    bool GraphAttachment::Init(const drv::ImageView::Descriptor& des)
    {
        descriptor = des;
        return true;
    }

    bool GraphAttachment::Compile()
    {
        auto res = image->CreateImageView(descriptor);
        if (!res) {
            return false;
        }
        view = res;
        return true;
    }

    bool GraphAttachment::operator==(const drv::ImageView::Descriptor& des)
    {
        return memcmp(&descriptor, &des, sizeof(drv::ImageView::Descriptor)) == 0;
    }

    const drv::ImageViewPtr& GraphAttachment::GetImageView() const
    {
        return view;
    }
}