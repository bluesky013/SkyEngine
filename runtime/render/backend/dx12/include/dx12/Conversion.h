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
    DXGI_FORMAT FromRHI(rhi::Format format);
    D3D12_RESOURCE_FLAGS FromRHI(const rhi::ImageUsageFlags &flags);
    D3D12_DESCRIPTOR_RANGE_TYPE FromRHI(rhi::DescriptorType type);
    D3D12_BLEND FromRHI(rhi::BlendFactor factor);
    D3D12_BLEND_OP FromRHI(rhi::BlendOp op);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE FromRHI(rhi::PrimitiveTopology topo);
    D3D12_FILL_MODE FromRHI(rhi::PolygonMode mode);
    D3D12_CULL_MODE FromRHI(const rhi::CullingModeFlags& cullMode);
    D3D12_COMPARISON_FUNC FromRHI(rhi::CompareOp compare);
    D3D12_STENCIL_OP FromRHI(rhi::StencilOp op);
    D3D12_SHADER_VISIBILITY FromRHI(const rhi::ShaderStageFlags &flags);

} // namespace sky::dx
