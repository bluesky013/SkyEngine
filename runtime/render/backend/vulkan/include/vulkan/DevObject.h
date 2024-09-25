//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include <core/template/ReferenceObject.h>
#include <memory>
#include <vulkan/Basic.h>

namespace sky::vk {

    class Device;

    class DevObject {
    public:
        explicit DevObject(Device &dev) : device(dev)
        {
        }
        virtual ~DevObject() = default;

        Device &GetDevice() const { return device; }

    protected:
        Device &device;
    };
    using DevPtr = std::shared_ptr<DevObject>;

} // namespace sky::vk
