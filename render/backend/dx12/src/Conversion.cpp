//
// Created by blues on 2023/10/11.
//

#include <dx12/Conversion.h>
#include <unordered_map>

namespace sky::dx {

    std::unordered_map<rhi::PixelFormat, DXGI_FORMAT> PIXEL_FORMAT_TABLE = {
        {rhi::PixelFormat::UNDEFINED,                 DXGI_FORMAT_UNKNOWN},
        {rhi::PixelFormat::R8_UNORM,                  DXGI_FORMAT_R8_UNORM},
        {rhi::PixelFormat::RGBA8_UNORM,               DXGI_FORMAT_R8G8B8A8_UNORM},
        {rhi::PixelFormat::RGBA8_SRGB,                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
        {rhi::PixelFormat::BGRA8_UNORM,               DXGI_FORMAT_B8G8R8A8_UNORM},
        {rhi::PixelFormat::BGRA8_SRGB,                DXGI_FORMAT_B8G8R8A8_UNORM_SRGB},
        {rhi::PixelFormat::R16_UNORM,                 DXGI_FORMAT_R16_UNORM},
        {rhi::PixelFormat::R32_SFLOAT,                DXGI_FORMAT_R32_FLOAT},
        {rhi::PixelFormat::D32,                       DXGI_FORMAT_D32_FLOAT},
        {rhi::PixelFormat::D24_S8,                    DXGI_FORMAT_D24_UNORM_S8_UINT},
        {rhi::PixelFormat::D32_S8,                    DXGI_FORMAT_D32_FLOAT_S8X24_UINT},
        {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK,       DXGI_FORMAT_BC1_UNORM},
        {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK,        DXGI_FORMAT_BC1_UNORM_SRGB},
        {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK,      DXGI_FORMAT_BC1_TYPELESS},
        {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK,       DXGI_FORMAT_BC1_TYPELESS},
        {rhi::PixelFormat::BC2_UNORM_BLOCK,           DXGI_FORMAT_BC2_UNORM},
        {rhi::PixelFormat::BC2_SRGB_BLOCK,            DXGI_FORMAT_BC2_UNORM_SRGB},
        {rhi::PixelFormat::BC3_UNORM_BLOCK,           DXGI_FORMAT_BC3_UNORM},
        {rhi::PixelFormat::BC3_SRGB_BLOCK,            DXGI_FORMAT_BC3_UNORM_SRGB},
        {rhi::PixelFormat::BC4_UNORM_BLOCK,           DXGI_FORMAT_BC4_UNORM},
        {rhi::PixelFormat::BC4_SNORM_BLOCK,           DXGI_FORMAT_BC4_SNORM},
        {rhi::PixelFormat::BC5_UNORM_BLOCK,           DXGI_FORMAT_BC5_UNORM},
        {rhi::PixelFormat::BC5_SNORM_BLOCK,           DXGI_FORMAT_BC5_SNORM},
        {rhi::PixelFormat::BC6H_UFLOAT_BLOCK,         DXGI_FORMAT_BC6H_UF16},
        {rhi::PixelFormat::BC6H_SFLOAT_BLOCK,         DXGI_FORMAT_BC6H_SF16},
        {rhi::PixelFormat::BC7_UNORM_BLOCK,           DXGI_FORMAT_BC7_UNORM},
        {rhi::PixelFormat::BC7_SRGB_BLOCK,            DXGI_FORMAT_BC7_UNORM_SRGB},
    };

    D3D12_HEAP_TYPE FromRHI(rhi::MemoryType type)
    {
        if (type == rhi::MemoryType::CPU_ONLY) {
            return D3D12_HEAP_TYPE_UPLOAD;
        }
        if (type == rhi::MemoryType::GPU_TO_CPU) {
            return D3D12_HEAP_TYPE_READBACK;
        }
        return D3D12_HEAP_TYPE_DEFAULT;
    }

    D3D12_RESOURCE_DIMENSION FromRHI(rhi::ImageType type)
    {
        if (type == rhi::ImageType::IMAGE_1D) {
            return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        }
        if (type == rhi::ImageType::IMAGE_3D) {
            return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        }
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }

    DXGI_FORMAT FromRHI(rhi::PixelFormat format)
    {
        auto iter = PIXEL_FORMAT_TABLE.find(format);
        return iter == PIXEL_FORMAT_TABLE.end() ? DXGI_FORMAT_UNKNOWN : iter->second;
    }

    D3D12_RESOURCE_FLAGS FromRHI(const rhi::ImageUsageFlags &flags)
    {
        D3D12_RESOURCE_FLAGS res = D3D12_RESOURCE_FLAG_NONE;
        if (flags & rhi::ImageUsageFlagBit::RENDER_TARGET) {
            res |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        if (flags & rhi::ImageUsageFlagBit::DEPTH_STENCIL) {
            res |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }
        if (flags & rhi::ImageUsageFlagBit::STORAGE) {
            res |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        if (!(flags & rhi::ImageUsageFlagBit::SAMPLED)) {
            res |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
        }
        return res;
    }

    D3D12_DESCRIPTOR_RANGE_TYPE FromRHI(rhi::DescriptorType type)
    {
        if (type == rhi::DescriptorType::UNIFORM_BUFFER ||
            type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        }
        if (type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER ||
            type == rhi::DescriptorType::SAMPLED_IMAGE) {
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }
        if (type == rhi::DescriptorType::SAMPLER) {
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        }
        if (type == rhi::DescriptorType::STORAGE_BUFFER ||
            type == rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC ||
            type == rhi::DescriptorType::STORAGE_IMAGE) {
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        }

        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }

} // namespace sky::dx