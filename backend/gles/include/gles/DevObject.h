//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include <core/template/ReferenceObject.h>
#include <memory>
#include <gles/Forward.h>

namespace sky::gles {

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

    static constexpr uint32_t INVALID_INDEX = ~(0U);

} // namespace sky::gles
