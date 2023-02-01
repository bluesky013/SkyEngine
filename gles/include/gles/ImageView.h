//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/ImageView.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView(Device &dev) : DevObject(dev) {}
        ~ImageView() = default;

        bool Init(const Descriptor &desc);
    };

}
