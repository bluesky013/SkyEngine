//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <RHI/Device.h>

#include <dx12/Basic.h>
#include <dx12/Queue.h>

namespace sky::dx {
    class Instance;

    class Device : public rhi::Device {
    public:
        ~Device() override = default;

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

        ID3D12Device *GetDevice() const;

        IDXGIFactory2 *GetDXGIFactory() const;

        Queue *GetGraphicsQueue() const;

    private:
        friend class Instance;
        bool Init(const Descriptor &, ComPtr<IDXGIAdapter1> &adaptor);

        Device(Instance &);
        Instance &instance;

        ComPtr<ID3D12Device> device;
        QueuePtr graphicsQueue;
    };

}