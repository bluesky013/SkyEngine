//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <memory>

namespace sky::rhi {

    class Device {
    public:
        Device() = default;
        virtual ~Device() = default;

        struct DeviceFeature {
            bool sparseBinding      = false;
            bool descriptorIndexing = false;
        };

        struct Descriptor {
            DeviceFeature feature;
        };

        template <typename T>
        inline std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

    protected:
        DeviceFeature enabledFeature;
    };

}
