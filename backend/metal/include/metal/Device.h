//
// Created by Zach Lee on 2022/11/4.
//

#pragma once

#include <Metal/Metal.hpp>
#include <memory>

namespace sky::mtl {
    class Instance;

    class Device {
    public:
        ~Device() = default;

        struct Descriptor {
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

        MTL::Device *GetMetalDevice() const;

    private:
        bool Init(const Descriptor &);

        friend class Instance;
        Device(Instance &);
        Instance    &instance;
        MTL::Device *device = nullptr;
    };
}
