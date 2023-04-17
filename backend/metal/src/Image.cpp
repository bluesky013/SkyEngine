//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Image.h>
#include <mtl/ImageView.h>

namespace sky::mtl {

    Image::Image(Device &dev) : DevObject(dev)
    {
    }

    bool Image::Init(const Descriptor &desc)
    {
        imageDesc = desc;
        return true;
    }

    rhi::ImageViewPtr Image::CreateView(const rhi::ImageViewDesc &desc)
    {
        ImageViewPtr ret = std::make_shared<ImageView>(device);
        ret->source      = shared_from_this();
        if (!ret->Init(desc)) {
            ret = nullptr;
        }
        return std::static_pointer_cast<rhi::ImageView>(ret);
    }

}
