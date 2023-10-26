//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/DescriptorSetPool.h>
#include <dx12/Device.h>

namespace sky::dx {

    DescriptorSetPool::DescriptorSetPool(Device &dev) : DevObject(dev)
    {
    }

    bool DescriptorSetPool::Init(const Descriptor &desc)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NodeMask = 1;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        uint32_t cbvSrvUavCount = 0;
        uint32_t samplerCount = 0;
        for (uint32_t i = 0; i < desc.sizeCount; ++i) {
            const auto &size = desc.sizeData[i];
            if (size.type == rhi::DescriptorType::SAMPLED_IMAGE ||
                size.type == rhi::DescriptorType::STORAGE_IMAGE ||
                size.type == rhi::DescriptorType::UNIFORM_BUFFER ||
                size.type == rhi::DescriptorType::STORAGE_BUFFER ||
                size.type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC ||
                size.type == rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC) {
                cbvSrvUavCount += size.count;
            } else if (size.type == rhi::DescriptorType::SAMPLER) {
                samplerCount += size.count;
            }
        }

        bool res = true;
        {
            heapDesc.NumDescriptors = cbvSrvUavCount;
            res &= SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(resHeap.GetAddressOf())));
        }

        {
            heapDesc.NumDescriptors = samplerCount;
            res &= SUCCEEDED(device.GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(samplerHeap.GetAddressOf())));
        }

        return res;
    }

    rhi::DescriptorSetPtr DescriptorSetPool::Allocate(const rhi::DescriptorSet::Descriptor &desc)
    {
        return {};
    }
}