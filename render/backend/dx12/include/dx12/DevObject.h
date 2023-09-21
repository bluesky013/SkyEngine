//
// Created by Zach Lee on 2021/11/7.
//

#pragma once

#include <memory>
#include <dx12/Base.h>

namespace sky::dx {

    class Device;

    class DevObject {
    public:
        DevObject(Device &dev) : device(dev)
        {
        }
        virtual ~DevObject() = default;

    protected:
        Device &device;
    };
    using DevPtr = std::shared_ptr<DevObject>;

} // namespace sky::vk
