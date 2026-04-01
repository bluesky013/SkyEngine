//
// Created by Zach Lee on 2026/3/31.
//

#include <D3D12Conversion.h>

namespace sky::aurora {

    // ---- PixelFormat ----
    static const DXGI_FORMAT PIXEL_FORMAT_TABLE[] = {
        DXGI_FORMAT_UNKNOWN,                   // UNDEFINED
        DXGI_FORMAT_R8_UINT,                   // R8_UINT
        DXGI_FORMAT_R8_UNORM,                  // R8_UNORM
        DXGI_FORMAT_R8_TYPELESS,               // R8_SRGB (no DXGI SRGB for R8)
        DXGI_FORMAT_R8G8B8A8_UNORM,            // RGBA8_UNORM
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,       // RGBA8_SRGB
        DXGI_FORMAT_B8G8R8A8_UNORM,            // BGRA8_UNORM
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,       // BGRA8_SRGB
        DXGI_FORMAT_R16_UNORM,                 // R16_UNORM
        DXGI_FORMAT_R16G16_UNORM,              // RG16_UNORM
        DXGI_FORMAT_R16G16B16A16_UNORM,        // RGBA16_UNORM
        DXGI_FORMAT_R16_FLOAT,                 // R16_SFLOAT
        DXGI_FORMAT_R16G16_FLOAT,              // RG16_SFLOAT
        DXGI_FORMAT_R16G16B16A16_FLOAT,        // RGBA16_SFLOAT
        DXGI_FORMAT_R32_FLOAT,                 // R32_SFLOAT
        DXGI_FORMAT_R32G32_FLOAT,              // RG32_SFLOAT
        DXGI_FORMAT_R32G32B32_FLOAT,           // RGB32_SFLOAT
        DXGI_FORMAT_R32G32B32A32_FLOAT,        // RGBA32_SFLOAT
        DXGI_FORMAT_R32_UINT,                  // R32_UINT
        DXGI_FORMAT_R32G32_UINT,               // RG32_UINT
        DXGI_FORMAT_R32G32B32_UINT,            // RGB32_UINT
        DXGI_FORMAT_R32G32B32A32_UINT,         // RGBA32_UINT
        DXGI_FORMAT_D32_FLOAT,                 // D32
        DXGI_FORMAT_D24_UNORM_S8_UINT,         // D24_S8
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,      // D32_S8
        DXGI_FORMAT_BC1_UNORM,                 // BC1_RGB_UNORM_BLOCK
        DXGI_FORMAT_BC1_UNORM_SRGB,            // BC1_RGB_SRGB_BLOCK
        DXGI_FORMAT_BC1_UNORM,                 // BC1_RGBA_UNORM_BLOCK
        DXGI_FORMAT_BC1_UNORM_SRGB,            // BC1_RGBA_SRGB_BLOCK
        DXGI_FORMAT_BC2_UNORM,                 // BC2_UNORM_BLOCK
        DXGI_FORMAT_BC2_UNORM_SRGB,            // BC2_SRGB_BLOCK
        DXGI_FORMAT_BC3_UNORM,                 // BC3_UNORM_BLOCK
        DXGI_FORMAT_BC3_UNORM_SRGB,            // BC3_SRGB_BLOCK
        DXGI_FORMAT_BC4_UNORM,                 // BC4_UNORM_BLOCK
        DXGI_FORMAT_BC4_SNORM,                 // BC4_SNORM_BLOCK
        DXGI_FORMAT_BC5_UNORM,                 // BC5_UNORM_BLOCK
        DXGI_FORMAT_BC5_SNORM,                 // BC5_SNORM_BLOCK
        DXGI_FORMAT_BC6H_UF16,                 // BC6H_UFLOAT_BLOCK
        DXGI_FORMAT_BC6H_SF16,                 // BC6H_SFLOAT_BLOCK
        DXGI_FORMAT_BC7_UNORM,                 // BC7_UNORM_BLOCK
        DXGI_FORMAT_BC7_UNORM_SRGB,            // BC7_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8_UNORM_BLOCK (unsupported)
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8A1_UNORM_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8A1_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8A8_UNORM_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ETC2_R8G8B8A8_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_4x4_UNORM_BLOCK (unsupported)
        DXGI_FORMAT_UNKNOWN,                   // ASTC_4x4_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_8x8_UNORM_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_8x8_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_10x10_UNORM_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_10x10_SRGB_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_12x12_UNORM_BLOCK
        DXGI_FORMAT_UNKNOWN,                   // ASTC_12x12_SRGB_BLOCK
    };

    DXGI_FORMAT FromPixelFormat(PixelFormat format)
    {
        const auto idx = static_cast<uint32_t>(format);
        if (idx < static_cast<uint32_t>(PixelFormat::MAX)) {
            return PIXEL_FORMAT_TABLE[idx];
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    // ---- Format (vertex attribute) ----
    static const DXGI_FORMAT FORMAT_TABLE[] = {
        DXGI_FORMAT_UNKNOWN,              // UNDEFINED
        DXGI_FORMAT_R32_FLOAT,            // F_R32
        DXGI_FORMAT_R32G32_FLOAT,         // F_RG32
        DXGI_FORMAT_R32G32B32_FLOAT,      // F_RGB32
        DXGI_FORMAT_R32G32B32A32_FLOAT,   // F_RGBA32
        DXGI_FORMAT_R8_UNORM,             // F_R8
        DXGI_FORMAT_R8G8_UNORM,           // F_RG8
        DXGI_FORMAT_UNKNOWN,              // F_RGB8 (no 3-channel in DXGI)
        DXGI_FORMAT_R8G8B8A8_UNORM,       // F_RGBA8
        DXGI_FORMAT_R8_UINT,              // U_R8
        DXGI_FORMAT_R8G8_UINT,            // U_RG8
        DXGI_FORMAT_UNKNOWN,              // U_RGB8 (no 3-channel in DXGI)
        DXGI_FORMAT_R8G8B8A8_UINT,        // U_RGBA8
        DXGI_FORMAT_R16_UINT,             // U_R16
        DXGI_FORMAT_R16G16_UINT,          // U_RG16
        DXGI_FORMAT_UNKNOWN,              // U_RGB16 (no 3-channel in DXGI)
        DXGI_FORMAT_R16G16B16A16_UINT,    // U_RGBA16
        DXGI_FORMAT_R32_UINT,             // U_R32
        DXGI_FORMAT_R32G32_UINT,          // U_RG32
        DXGI_FORMAT_R32G32B32_UINT,       // U_RGB32
        DXGI_FORMAT_R32G32B32A32_UINT,    // U_RGBA32
    };

    DXGI_FORMAT FromFormat(Format format)
    {
        const auto idx = static_cast<uint32_t>(format);
        if (idx < sizeof(FORMAT_TABLE) / sizeof(FORMAT_TABLE[0])) {
            return FORMAT_TABLE[idx];
        }
        return DXGI_FORMAT_UNKNOWN;
    }

    // ---- ImageType ----
    D3D12_RESOURCE_DIMENSION FromImageType(ImageType type)
    {
        static const D3D12_RESOURCE_DIMENSION TABLE[] = {
            D3D12_RESOURCE_DIMENSION_TEXTURE1D, // IMAGE_1D
            D3D12_RESOURCE_DIMENSION_TEXTURE2D, // IMAGE_2D
            D3D12_RESOURCE_DIMENSION_TEXTURE3D, // IMAGE_3D
        };
        return TABLE[static_cast<uint32_t>(type)];
    }

    // ---- MemoryType ----
    D3D12_HEAP_TYPE FromMemoryType(MemoryType type)
    {
        switch (type) {
        case MemoryType::CPU_ONLY:
        case MemoryType::CPU_TO_GPU: return D3D12_HEAP_TYPE_UPLOAD;
        case MemoryType::GPU_TO_CPU: return D3D12_HEAP_TYPE_READBACK;
        default:                     return D3D12_HEAP_TYPE_DEFAULT;
        }
    }

    // ---- ImageUsageFlags ----
    D3D12_RESOURCE_FLAGS FromImageUsageFlags(const ImageUsageFlags &flags)
    {
        D3D12_RESOURCE_FLAGS res = D3D12_RESOURCE_FLAG_NONE;
        if (flags & ImageUsageFlagBit::RENDER_TARGET)  { res |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; }
        if (flags & ImageUsageFlagBit::DEPTH_STENCIL)  { res |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; }
        if (flags & ImageUsageFlagBit::STORAGE)        { res |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; }
        if (!(flags & ImageUsageFlagBit::SAMPLED))     { res |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE; }
        return res;
    }

    // ---- BlendFactor ----
    static const D3D12_BLEND BLEND_TABLE[] = {
        D3D12_BLEND_ZERO,              // ZERO
        D3D12_BLEND_ONE,               // ONE
        D3D12_BLEND_SRC_COLOR,         // SRC_COLOR
        D3D12_BLEND_INV_SRC_COLOR,     // ONE_MINUS_SRC_COLOR
        D3D12_BLEND_DEST_COLOR,        // DST_COLOR
        D3D12_BLEND_INV_DEST_COLOR,    // ONE_MINUS_DST_COLOR
        D3D12_BLEND_SRC_ALPHA,         // SRC_ALPHA
        D3D12_BLEND_INV_SRC_ALPHA,     // ONE_MINUS_SRC_ALPHA
        D3D12_BLEND_DEST_ALPHA,        // DST_ALPHA
        D3D12_BLEND_INV_DEST_ALPHA,    // ONE_MINUS_DST_ALPHA
        D3D12_BLEND_BLEND_FACTOR,      // CONSTANT_COLOR
        D3D12_BLEND_INV_BLEND_FACTOR,  // ONE_MINUS_CONSTANT_COLOR
        D3D12_BLEND_ALPHA_FACTOR,      // CONSTANT_ALPHA
        D3D12_BLEND_INV_ALPHA_FACTOR,  // ONE_MINUS_CONSTANT_ALPHA
        D3D12_BLEND_SRC_ALPHA_SAT,     // SRC_ALPHA_SATURATE
        D3D12_BLEND_SRC1_COLOR,        // SRC1_COLOR
        D3D12_BLEND_INV_SRC1_COLOR,    // ONE_MINUS_SRC1_COLOR
        D3D12_BLEND_SRC1_ALPHA,        // SRC1_ALPHA
        D3D12_BLEND_INV_SRC1_ALPHA,    // ONE_MINUS_SRC1_ALPHA
    };

    D3D12_BLEND FromBlendFactor(BlendFactor factor)
    {
        return BLEND_TABLE[static_cast<uint32_t>(factor)];
    }

    // ---- BlendOp ----
    D3D12_BLEND_OP FromBlendOp(BlendOp op)
    {
        return op == BlendOp::ADD ? D3D12_BLEND_OP_ADD : D3D12_BLEND_OP_SUBTRACT;
    }

    // ---- CompareOp ----
    static const D3D12_COMPARISON_FUNC COMPARE_TABLE[] = {
        D3D12_COMPARISON_FUNC_NEVER,          // NEVER
        D3D12_COMPARISON_FUNC_LESS,           // LESS
        D3D12_COMPARISON_FUNC_EQUAL,          // EQUAL
        D3D12_COMPARISON_FUNC_LESS_EQUAL,     // LESS_OR_EQUAL
        D3D12_COMPARISON_FUNC_GREATER,        // GREATER
        D3D12_COMPARISON_FUNC_NOT_EQUAL,      // NOT_EQUAL
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,  // GREATER_OR_EQUAL
        D3D12_COMPARISON_FUNC_ALWAYS,         // ALWAYS
    };

    D3D12_COMPARISON_FUNC FromCompareOp(CompareOp op)
    {
        return COMPARE_TABLE[static_cast<uint32_t>(op)];
    }

    // ---- StencilOp ----
    static const D3D12_STENCIL_OP STENCIL_TABLE[] = {
        D3D12_STENCIL_OP_KEEP,      // KEEP
        D3D12_STENCIL_OP_ZERO,      // ZERO
        D3D12_STENCIL_OP_REPLACE,   // REPLACE
        D3D12_STENCIL_OP_INCR_SAT,  // INCREMENT_AND_CLAMP
        D3D12_STENCIL_OP_DECR_SAT,  // DECREMENT_AND_CLAMP
        D3D12_STENCIL_OP_INVERT,    // INVERT
        D3D12_STENCIL_OP_INCR,      // INCREMENT_AND_WRAP
        D3D12_STENCIL_OP_DECR,      // DECREMENT_AND_WRAP
    };

    D3D12_STENCIL_OP FromStencilOp(StencilOp op)
    {
        return STENCIL_TABLE[static_cast<uint32_t>(op)];
    }

    // ---- PrimitiveTopology ----
    static const D3D12_PRIMITIVE_TOPOLOGY_TYPE TOPO_TABLE[] = {
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,     // POINT_LIST
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,      // LINE_LIST
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,      // LINE_STRIP
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,  // TRIANGLE_LIST
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,  // TRIANGLE_STRIP
        D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED, // TRIANGLE_FAN (unsupported)
    };

    D3D12_PRIMITIVE_TOPOLOGY_TYPE FromPrimitiveTopology(PrimitiveTopology topo)
    {
        return TOPO_TABLE[static_cast<uint32_t>(topo)];
    }

    // ---- PolygonMode ----
    D3D12_FILL_MODE FromPolygonMode(PolygonMode mode)
    {
        return mode == PolygonMode::FILL ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    }

    // ---- CullMode ----
    D3D12_CULL_MODE FromCullMode(const CullingModeFlags &flags)
    {
        if (flags & CullModeFlagBits::BACK)  { return D3D12_CULL_MODE_BACK; }
        if (flags & CullModeFlagBits::FRONT) { return D3D12_CULL_MODE_FRONT; }
        return D3D12_CULL_MODE_NONE;
    }

    // ---- IndexType ----
    DXGI_FORMAT FromIndexType(IndexType type)
    {
        return type == IndexType::U16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    }

    // ---- DescriptorType ----
    D3D12_DESCRIPTOR_RANGE_TYPE FromDescriptorType(DescriptorType type)
    {
        switch (type) {
        case DescriptorType::UNIFORM_BUFFER:
        case DescriptorType::UNIFORM_BUFFER_DYNAMIC:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case DescriptorType::COMBINED_IMAGE_SAMPLER:
        case DescriptorType::SAMPLED_IMAGE:
        case DescriptorType::INPUT_ATTACHMENT:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::SAMPLER:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case DescriptorType::STORAGE_BUFFER:
        case DescriptorType::STORAGE_BUFFER_DYNAMIC:
        case DescriptorType::STORAGE_IMAGE:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        default:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        }
    }

    // ---- ShaderStageFlags ----
    D3D12_SHADER_VISIBILITY FromShaderStageFlags(const ShaderStageFlags &flags)
    {
        const auto gfx = ShaderStageFlagBit::VS | ShaderStageFlagBit::FS;
        if (flags == ShaderStageFlagBit::VS)  { return D3D12_SHADER_VISIBILITY_VERTEX; }
        if (flags == ShaderStageFlagBit::FS)  { return D3D12_SHADER_VISIBILITY_PIXEL; }
        if (flags == ShaderStageFlagBit::MS)  { return D3D12_SHADER_VISIBILITY_MESH; }
        if (flags == ShaderStageFlagBit::TAS) { return D3D12_SHADER_VISIBILITY_AMPLIFICATION; }
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    // ---- StencilState ----
    D3D12_DEPTH_STENCILOP_DESC FromStencilState(const StencilState &state)
    {
        D3D12_DEPTH_STENCILOP_DESC face = {};
        face.StencilFailOp      = FromStencilOp(state.failOp);
        face.StencilDepthFailOp = FromStencilOp(state.depthFailOp);
        face.StencilPassOp      = FromStencilOp(state.passOp);
        face.StencilFunc        = FromCompareOp(state.compareOp);
        return face;
    }

    // ---- BufferUsageFlags ----
    D3D12_RESOURCE_FLAGS FromBufferUsageFlags(const BufferUsageFlags &flags)
    {
        D3D12_RESOURCE_FLAGS res = D3D12_RESOURCE_FLAG_NONE;
        if (flags & BufferUsageFlagBit::STORAGE) {
            res |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }
        return res;
    }

    // ---- Sampler Filter ----
    D3D12_FILTER FromFilter(Filter min, Filter mag, MipFilter mip, bool aniso)
    {
        if (aniso) {
            return D3D12_FILTER_ANISOTROPIC;
        }
        // encode: bit0 = mip, bit2 = mag, bit4 = min
        //   NEAREST=0, LINEAR=1
        const UINT val = (static_cast<UINT>(min) << 4)
                       | (static_cast<UINT>(mag) << 2)
                       | (static_cast<UINT>(mip));
        return static_cast<D3D12_FILTER>(val);
    }

    // ---- WrapMode ----
    D3D12_TEXTURE_ADDRESS_MODE FromWrapMode(WrapMode mode)
    {
        static const D3D12_TEXTURE_ADDRESS_MODE TABLE[] = {
            D3D12_TEXTURE_ADDRESS_MODE_WRAP,        // REPEAT
            D3D12_TEXTURE_ADDRESS_MODE_MIRROR,      // MIRRORED_REPEAT
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,       // CLAMP_TO_EDGE
            D3D12_TEXTURE_ADDRESS_MODE_BORDER,      // CLAMP_TO_BORDER
            D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE, // MIRROR_CLAMP_TO_EDGE
        };
        return TABLE[static_cast<uint32_t>(mode)];
    }

} // namespace sky::aurora