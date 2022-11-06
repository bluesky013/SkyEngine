//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class DescriptorPool : public DevObject {
    public:
        DescriptorPool(Device &dev);
        ~DescriptorPool();

        struct Descriptor {
        };

        ID3D12DescriptorHeap *GetHeap() const;

    private:
        friend class Device;
        bool Init(const Descriptor &);

        ComPtr<ID3D12DescriptorHeap> heap;
    };

}