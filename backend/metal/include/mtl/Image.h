//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/Image.h>
#include <mtl/DevObject.h>
#include <Metal/MTLTexture.hpp>

namespace sky::mtl {

    class Image : public rhi::Image, public DevObject, public std::enable_shared_from_this<Image> {
    public:
        Image() = default;
        ~Image() = default;

        rhi::ImageViewPtr CreateView(const rhi::ImageViewDesc &desc);
    private:
        friend class Device;
        Image(Device &);
        bool Init(const Descriptor &);
    };
    using ImagePtr = std::shared_ptr<Image>;
}
