//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <d3d12.h>

namespace sky::aurora {

    // pixel / vertex format
    DXGI_FORMAT                FromPixelFormat(PixelFormat format);
    DXGI_FORMAT                FromFormat(Format format);

    // image
    D3D12_RESOURCE_DIMENSION   FromImageType(ImageType type);
    D3D12_RESOURCE_FLAGS       FromImageUsageFlags(const ImageUsageFlags &flags);

    // memory
    D3D12_HEAP_TYPE            FromMemoryType(MemoryType type);

    // buffer
    D3D12_RESOURCE_FLAGS       FromBufferUsageFlags(const BufferUsageFlags &flags);

    // pipeline state
    D3D12_BLEND                FromBlendFactor(BlendFactor factor);
    D3D12_BLEND_OP             FromBlendOp(BlendOp op);
    D3D12_COMPARISON_FUNC      FromCompareOp(CompareOp op);
    D3D12_STENCIL_OP           FromStencilOp(StencilOp op);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE FromPrimitiveTopology(PrimitiveTopology topo);
    D3D12_FILL_MODE            FromPolygonMode(PolygonMode mode);
    D3D12_CULL_MODE            FromCullMode(const CullingModeFlags &flags);
    DXGI_FORMAT                FromIndexType(IndexType type);
    D3D12_DEPTH_STENCILOP_DESC FromStencilState(const StencilState &state);

    // descriptor
    D3D12_DESCRIPTOR_RANGE_TYPE FromDescriptorType(DescriptorType type);

    // shader
    D3D12_SHADER_VISIBILITY    FromShaderStageFlags(const ShaderStageFlags &flags);

    // sampler
    D3D12_FILTER               FromFilter(Filter min, Filter mag, MipFilter mip, bool aniso);
    D3D12_TEXTURE_ADDRESS_MODE FromWrapMode(WrapMode mode);

} // namespace sky::aurora