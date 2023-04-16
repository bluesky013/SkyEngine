//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Image.h>

namespace sky::mtl {

    Image::Image(Device &dev) : DevObject(dev)
    {
    }

    bool Image::Init(const Descriptor &desc)
    {
        imageDesc = desc;
    }

}
