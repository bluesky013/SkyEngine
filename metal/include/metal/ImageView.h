//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <metal/DevObject.h>

namespace sky::mtl {

    class ImageView : public DevObject {
    public:
        ImageView() = default;
        ~ImageView() = default;

    private:
        Device &device;
    };
}
