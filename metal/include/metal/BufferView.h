//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <rhi/BufferView.h>
#include <metal/DevObject.h>

namespace sky::mtl {

    class Device;

    class BufferView : public rhi::BufferView, public DevObject {
    public:
        BufferView() = default;
        ~BufferView() = default;

    private:
        Device &device;
    };

}
