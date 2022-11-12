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

        struct Descriptor {
            ImageViewType viewType = ImageViewType::VIEW_2D;
            ImageSubRange subRange = {0, 1, 0, 1};
            AspectFlags   mask = rhi::AspectFlagBit::COLOR_BIT;
        };

        const Descriptor &GetDescriptor() const;

    protected:
        Descriptor imageDesc;
    };

}