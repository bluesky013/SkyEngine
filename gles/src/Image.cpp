//
// Created by Zach on 2023/1/31.
//

#include <gles/Image.h>
#include <gles/Device.h>

namespace sky::gles {

    bool Image::Init(const Descriptor &desc)
    {
        glGenTextures(1, &texId);
    }

}
