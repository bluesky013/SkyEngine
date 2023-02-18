//
// Created by Zach Lee on 2022/11/5.
//

#pragma once

#include <rhi/DescriptorSetPool.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class DescriptorSetPool : public rhi::DescriptorSetPool, public DevObject {
    public:
        DescriptorSetPool(Device &dev);
        ~DescriptorSetPool();

        ID3D12DescriptorHeap *GetHeap() const;

    private:
        friend class Device;
        bool Init(const Descriptor &);

        ComPtr<ID3D12DescriptorHeap> heap;
    };

}