//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/ImageView.h>
#include <metal/DevObject.h>

namespace sky::mtl {

    class ImageView : public rhi::ImageView, public DevObject {
    public:
        ImageView() = default;
        ~ImageView() = default;

    private:
        Device &device;
    };
}
