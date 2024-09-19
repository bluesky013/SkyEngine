//
// Created by blues on 2023/10/11.
//

#include <dx12/Conversion.h>
#include <unordered_map>
#include <core/logger/Logger.h>

namespace sky::dx {
    static const char* TAG = "D3D12Conv";

    std::unordered_map<rhi::PixelFormat, DXGI_FORMAT> PIXEL_FORMAT_TABLE = {
        {rhi::PixelFormat::UNDEFINED,                 DXGI_FORMAT_UNKNOWN},
        {rhi::PixelFormat::R8_UNORM,                  DXGI_FORMAT_R8_UNORM},
        {rhi::PixelFormat::R8_SRGB,                   DXGI_FORMAT_R8_TYPELESS},
        {rhi::PixelFormat::RGBA8_UNORM,               DXGI_FORMAT_R8G8B8A8_UNORM},
        {rhi::PixelFormat::RGBA8_SRGB,                DXGI_FORMAT_R8G8B8A8_UNORM_SRGB},
        {rhi::PixelFormat::BGRA8_UNORM,               DXGI_FORMAT_B8G8R8A8_UNORM},
        {rhi::PixelFormat::BGRA8_SRGB,                DXGI_FORMAT_B8G8R8A8_UNORM_SRGB},
        {rhi::PixelFormat::R16_UNORM,                 DXGI_FORMAT_R16_UNORM},
        {rhi::PixelFormat::RGBA16_SFLOAT,             DXGI_FORMAT_R16G16B16A16_FLOAT},
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

    std::unordered_map<rhi::Format, DXGI_FORMAT> FORMAT_TABLE = {
        {rhi::Format::UNDEFINED, DXGI_FORMAT_UNKNOWN},
        {rhi::Format::F_R32,     DXGI_FORMAT_R32_FLOAT},
        {rhi::Format::F_RG32,    DXGI_FORMAT_R32G32_FLOAT},
        {rhi::Format::F_RGB32,   DXGI_FORMAT_R32G32B32_FLOAT},
        {rhi::Format::F_RGBA32,  DXGI_FORMAT_R32G32B32A32_FLOAT},
        {rhi::Format::U_R32,     DXGI_FORMAT_R32_UINT},
        {rhi::Format::U_RG32,    DXGI_FORMAT_R32G32_UINT},
        {rhi::Format::U_RGB32,   DXGI_FORMAT_R32G32B32_UINT},
        {rhi::Format::U_RGBA32,  DXGI_FORMAT_R32G32B32A32_UINT},
    };

    D3D12_BLEND BLEND_MAP[] = {
        D3D12_BLEND_ZERO,
        D3D12_BLEND_ONE,
        D3D12_BLEND_SRC_COLOR,
        D3D12_BLEND_INV_SRC_COLOR,
        D3D12_BLEND_DEST_COLOR,
        D3D12_BLEND_INV_DEST_COLOR,
        D3D12_BLEND_SRC_ALPHA,
        D3D12_BLEND_INV_SRC_ALPHA,
        D3D12_BLEND_DEST_ALPHA,
        D3D12_BLEND_INV_DEST_ALPHA,
        D3D12_BLEND_BLEND_FACTOR,
        D3D12_BLEND_INV_BLEND_FACTOR,
        D3D12_BLEND_ALPHA_FACTOR,
        D3D12_BLEND_INV_ALPHA_FACTOR,
        D3D12_BLEND_SRC_ALPHA_SAT,
        D3D12_BLEND_SRC1_COLOR,
        D3D12_BLEND_INV_SRC1_ALPHA,
        D3D12_BLEND_SRC1_ALPHA,
        D3D12_BLEND_INV_SRC1_ALPHA,
    };

    D3D12_PRIMITIVE_TOPOLOGY_TYPE PRIMITIVE_TOPO_MAP[] = {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
    };

    D3D12_COMPARISON_FUNC COMPARE_FUNC_MAP[] = {
        D3D12_COMPARISON_FUNC_NEVER,
        D3D12_COMPARISON_FUNC_LESS,
        D3D12_COMPARISON_FUNC_EQUAL,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER,
        D3D12_COMPARISON_FUNC_NOT_EQUAL,
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        D3D12_COMPARISON_FUNC_ALWAYS,
    };

    D3D12_STENCIL_OP STENCIL_OP_MAP[] = {
        D3D12_STENCIL_OP_KEEP,
        D3D12_STENCIL_OP_ZERO,
        D3D12_STENCIL_OP_REPLACE,
        D3D12_STENCIL_OP_INCR_SAT,
        D3D12_STENCIL_OP_DECR_SAT,
        D3D12_STENCIL_OP_INVERT,
        D3D12_STENCIL_OP_INCR,
        D3D12_STENCIL_OP_DECR,
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

    DXGI_FORMAT FromRHI(rhi::Format format)
    {
        auto iter = FORMAT_TABLE.find(format);
        return iter == FORMAT_TABLE.end() ? DXGI_FORMAT_UNKNOWN : iter->second;
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

    D3D12_BLEND FromRHI(rhi::BlendFactor factor)
    {
        return BLEND_MAP[static_cast<uint32_t>(factor)];
    }

    D3D12_BLEND_OP FromRHI(rhi::BlendOp op)
    {
        return op == rhi::BlendOp::ADD ? D3D12_BLEND_OP_ADD : D3D12_BLEND_OP_SUBTRACT;
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE FromRHI(rhi::PrimitiveTopology topo)
    {
        return PRIMITIVE_TOPO_MAP[static_cast<uint32_t>(topo)];
    }

    D3D12_FILL_MODE FromRHI(rhi::PolygonMode mode)
    {
        switch (mode) {
        case rhi::PolygonMode::FILL: return D3D12_FILL_MODE_SOLID;
        case rhi::PolygonMode::LINE: return D3D12_FILL_MODE_WIREFRAME;
        default:
            break;
        }
        LOG_W(TAG, "Polygon Mode not supported %d, fallback to %d", mode, D3D12_FILL_MODE_WIREFRAME);
        return D3D12_FILL_MODE_WIREFRAME;
    }

    D3D12_CULL_MODE FromRHI(const rhi::CullingModeFlags& cullMode)
    {
        if (cullMode & rhi::CullModeFlagBits::NONE) {
            return D3D12_CULL_MODE_NONE;
        }
        if (cullMode & rhi::CullModeFlagBits::BACK) {
            return D3D12_CULL_MODE_BACK;
        }
        return D3D12_CULL_MODE_FRONT;
    }

    D3D12_SHADER_VISIBILITY FromRHI(const rhi::ShaderStageFlags &flags)
    {
        static const rhi::ShaderStageFlags GRAPHICS_ALL = rhi::ShaderStageFlagBit::VS | rhi::ShaderStageFlagBit::FS;

        D3D12_SHADER_VISIBILITY visibility = {};

        if (flags == rhi::ShaderStageFlagBit::CS) {
            visibility = D3D12_SHADER_VISIBILITY_ALL;
        } else if (flags == GRAPHICS_ALL) {
            visibility = D3D12_SHADER_VISIBILITY_ALL;
        } else if (flags == rhi::ShaderStageFlagBit::VS) {
            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
        } else if (flags == rhi::ShaderStageFlagBit::FS) {
            visibility = D3D12_SHADER_VISIBILITY_VERTEX;
        }
        return visibility;
    }

    D3D12_COMPARISON_FUNC FromRHI(rhi::CompareOp compare)
    {
        return COMPARE_FUNC_MAP[static_cast<uint32_t>(compare)];
    }

    D3D12_STENCIL_OP FromRHI(rhi::StencilOp op)
    {
        return STENCIL_OP_MAP[static_cast<uint32_t>(op)];
    }

} // namespace sky::dx