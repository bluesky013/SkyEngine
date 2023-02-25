//
// Created by Zach Lee on 2022/11/12.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class ImageView {
    public:
        ImageView()          = default;
        virtual ~ImageView() = default;

        const ImageViewDesc &GetDescriptor() const { return viewDesc; }

    protected:
        ImageViewDesc viewDesc;
    };
    using ImageViewPtr = std::shared_ptr<ImageView>;
}