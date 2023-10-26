//
// Created by Zach Lee on 2023/2/19.
//

#pragma once

#include <rhi/Semaphore.h>
#include <gles/DevObject.h>

namespace sky::gles {
    class Device;

    class Semaphore : public rhi::Semaphore, public DevObject {
    public:
        Semaphore(Device &dev) : DevObject(dev) {}
        ~Semaphore() = default;

        bool Init(const Descriptor &desc) { return true; }
    };

}