//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Image.h>
#include <gles/DevObject.h>
#include <gles/Conversion.h>

namespace sky::gles {

    class Image : public rhi::Image, public DevObject {
    public:
        Image(Device &dev) : DevObject(dev) {}
        ~Image();

        bool Init(const Descriptor &desc);

    private:
        GLuint texId = 0;
        bool renderBuffer = false;
    };

}
