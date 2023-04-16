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

        const ImageViewDesc &GetViewDesc() const { return viewDesc; }

        virtual std::shared_ptr<ImageView> CreateView(const ImageViewDesc &desc) const = 0;

    protected:
        ImageViewDesc viewDesc;
    };
    using ImageViewPtr = std::shared_ptr<ImageView>;
}