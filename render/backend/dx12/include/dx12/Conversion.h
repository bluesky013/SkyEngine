//
// Created by blues on 2023/10/11.
//

#pragma once

#include <rhi/Core.h>
#include <d3d12.h>

namespace sky::dx {

    D3D12_HEAP_TYPE FromRHI(rhi::MemoryType type);
    D3D12_RESOURCE_DIMENSION FromRHI(rhi::ImageType type);
    DXGI_FORMAT FromRHI(rhi::PixelFormat format);
    D3D12_RESOURCE_FLAGS FromRHI(const rhi::ImageUsageFlags &flags);
    D3D12_DESCRIPTOR_RANGE_TYPE FromRHI(rhi::DescriptorType type);

} // namespace sky::dx
