//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/Image.h>
#include <dx12/DevObject.h>

namespace sky::dx {

    class Image : public rhi::Image, public DevObject {
    public:
        Image(Device &dev);
        ~Image() override;
    };
    using ImagePtr = std::shared_ptr<Image>;
}