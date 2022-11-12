//
// Created by Zach Lee on 2022/11/10.
//

#include <rhi/Image.h>

namespace sky::rhi {

    const Image::Descriptor &Image::GetDescriptor() const
    {
        return imageDesc;
    }

}