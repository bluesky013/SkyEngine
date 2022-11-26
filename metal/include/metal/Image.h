//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Image.h>
#include <metal/DevObject.h>
#include <Metal/MTLTexture.hpp>

namespace sky::mtl {

    class Image : public rhi::Image, public DevObject {
    public:
        Image() = default;
        ~Image() = default;

    private:
        Image(Device &);
        bool Init(const Descriptor &);
    };

}
