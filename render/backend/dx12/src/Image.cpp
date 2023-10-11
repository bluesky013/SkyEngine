//
// Created by Zach Lee on 2022/11/5.
//

#include <dx12/Image.h>
#include <dx12/Device.h>
#include <dx12/Conversion.h>

namespace sky::dx {

    Image::Image(Device &dev) : DevObject(dev)
    {
    }

    bool Image::Init(const Descriptor &desc)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = FromRHI(desc.memory);
        heapProperties.CPUPageProperty;
        heapProperties.MemoryPoolPreference;
        heapProperties.CreationNodeMask;
        heapProperties.VisibleNodeMask;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = FromRHI(desc.imageType);
        resourceDesc.Alignment = 0;
        resourceDesc.Width     = desc.extent.width;
        resourceDesc.Height    = desc.extent.height;
        resourceDesc.DepthOrArraySize = desc.imageType == rhi::ImageType::IMAGE_3D ? desc.extent.depth : desc.arrayLayers;
        resourceDesc.MipLevels  = desc.mipLevels;
        resourceDesc.Format     = FromRHI(desc.format);
        resourceDesc.SampleDesc = {static_cast<uint32_t>(desc.samples), 0};
        resourceDesc.Layout     = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags      = FromRHI(desc.usage);

        return !FAILED(device.GetDevice()->CreateCommittedResource(&heapProperties,
                                                                   D3D12_HEAP_FLAG_NONE,
                                                                   &resourceDesc,
                                                                   D3D12_RESOURCE_STATE_COMMON,
                                                                   nullptr,
                                                                   IID_PPV_ARGS(resource.GetAddressOf())));
    }

} // namespace sky::dx