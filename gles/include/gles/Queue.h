//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Queue.h>
#include <gles/DevObject.h>

namespace sky::gles {
    class Device;

    class Queue : public rhi::Queue, public DevObject {
    public:
        Queue(Device &dev) : DevObject(dev) {}
        ~Queue() = default;
    };

}
