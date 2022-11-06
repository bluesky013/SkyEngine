//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <metal/DevObject.h>

namespace sky::mtl {

    class Image : public DevObject {
    public:
        Image() = default;
        ~Image() = default;

    private:
        Device &device;
    };

}
