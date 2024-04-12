//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Buffer.h>
#include <dx12/Device.h>
#include <dx12/Conversion.h>

namespace sky::dx {

    Buffer::Buffer(Device &dev) : DevObject(dev)
    {
    }

    bool Buffer::Init(const Descriptor &desc)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = FromRHI(desc.memory);
        heapProperties.CPUPageProperty;
        heapProperties.MemoryPoolPreference;
        heapProperties.CreationNodeMask;
        heapProperties.VisibleNodeMask;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = desc.size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc = {};
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        return SUCCEEDED(device.GetDevice()->CreateCommittedResource(&heapProperties,
                                                    D3D12_HEAP_FLAG_NONE,
                                                    &resourceDesc,
                                                    D3D12_RESOURCE_STATE_COMMON,
                                                    nullptr,
                                                    IID_PPV_ARGS(resource.GetAddressOf())));
    }
}