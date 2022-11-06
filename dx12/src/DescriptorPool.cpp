//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/DescriptorPool.h>
#include <dx12/Device.h>

namespace sky::dx {

    DescriptorPool::DescriptorPool(Device &dev) : DevObject(dev)
    {
    }

    DescriptorPool::~DescriptorPool()
    {
    }

    bool DescriptorPool::Init(const Descriptor &)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};

        if (FAILED(device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(heap.GetAddressOf())))) {
            return false;
        }

        return true;
    }

    ID3D12DescriptorHeap *DescriptorPool::GetHeap() const
    {
        return heap.Get();
    }

}