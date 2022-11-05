//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <memory>
#include <dx12/Basic.h>

namespace sky::dx {
    class Instance;

    class Device {
    public:
        ~Device() = default;

        struct Descriptor {};

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

    private:
        friend class Instance;
        bool Init(const Descriptor &, CompPtr<IDXGIAdapter1> adaptor);

        Device(Instance &);
        Instance &instance;

        CompPtr<ID3D12Device> device;
    };

}