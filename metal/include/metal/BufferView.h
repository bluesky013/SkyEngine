//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <metal/DevObject.h>

namespace sky::mtl {

    class Device;

    class BufferView : public DevObject {
    public:
        BufferView() = default;
        ~BufferView() = default;

    private:
        Device &device;
    };

}
