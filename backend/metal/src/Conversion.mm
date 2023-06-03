//
// Created by Zach Lee on 2023/5/27.
//

#include <mtl/Conversion.h>
#include <unordered_map>
#include <core/platform/Platform.h>

namespace sky::mtl {

    std::unordered_map<rhi::PixelFormat, MTLPixelFormat> PIXEL_FORMAT_TABLE =
        {
            {rhi::PixelFormat::UNDEFINED, MTLPixelFormatInvalid},
            {rhi::PixelFormat::RGB8_UNORM, MTLPixelFormatInvalid},
            {rhi::PixelFormat::RGB8_SRGB, MTLPixelFormatInvalid},
            {rhi::PixelFormat::RGBA8_UNORM, MTLPixelFormatRGBA8Unorm},
            {rhi::PixelFormat::RGBA8_SRGB, MTLPixelFormatRGBA8Unorm_sRGB},
            {rhi::PixelFormat::BGRA8_UNORM, MTLPixelFormatBGRA8Unorm},
            {rhi::PixelFormat::BGRA8_SRGB, MTLPixelFormatBGRA8Unorm_sRGB},
            {rhi::PixelFormat::D32, MTLPixelFormatDepth32Float},
            {rhi::PixelFormat::D24_S8, MTLPixelFormatDepth24Unorm_Stencil8},
            {rhi::PixelFormat::D32_S8, MTLPixelFormatDepth32Float_Stencil8},
            {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK, MTLPixelFormatBC1_RGBA},
            {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK, MTLPixelFormatBC1_RGBA_sRGB},
            {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK, MTLPixelFormatBC1_RGBA},
            {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK, MTLPixelFormatBC1_RGBA_sRGB},
            {rhi::PixelFormat::BC2_UNORM_BLOCK, MTLPixelFormatBC2_RGBA},
            {rhi::PixelFormat::BC2_SRGB_BLOCK, MTLPixelFormatBC2_RGBA_sRGB},
            {rhi::PixelFormat::BC3_UNORM_BLOCK, MTLPixelFormatBC3_RGBA},
            {rhi::PixelFormat::BC3_SRGB_BLOCK, MTLPixelFormatBC3_RGBA_sRGB},
            {rhi::PixelFormat::BC4_UNORM_BLOCK, MTLPixelFormatBC4_RUnorm},
            {rhi::PixelFormat::BC4_SNORM_BLOCK, MTLPixelFormatBC4_RSnorm},
            {rhi::PixelFormat::BC5_UNORM_BLOCK, MTLPixelFormatBC5_RGUnorm},
            {rhi::PixelFormat::BC5_SNORM_BLOCK, MTLPixelFormatBC5_RGSnorm},
            {rhi::PixelFormat::BC6H_UFLOAT_BLOCK, MTLPixelFormatBC6H_RGBUfloat},
            {rhi::PixelFormat::BC6H_SFLOAT_BLOCK, MTLPixelFormatBC6H_RGBFloat},
            {rhi::PixelFormat::BC7_UNORM_BLOCK, MTLPixelFormatBC7_RGBAUnorm},
            {rhi::PixelFormat::BC7_SRGB_BLOCK, MTLPixelFormatBC7_RGBAUnorm_sRGB},
            {rhi::PixelFormat::ETC2_R8G8B8_UNORM_BLOCK, MTLPixelFormatETC2_RGB8},
            {rhi::PixelFormat::ETC2_R8G8B8_SRGB_BLOCK, MTLPixelFormatETC2_RGB8_sRGB},
            {rhi::PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, MTLPixelFormatETC2_RGB8A1},
            {rhi::PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK, MTLPixelFormatETC2_RGB8A1_sRGB},
            {rhi::PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, MTLPixelFormatInvalid},
            {rhi::PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK, MTLPixelFormatInvalid},
            {rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK, MTLPixelFormatASTC_4x4_LDR},
            {rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK, MTLPixelFormatASTC_4x4_sRGB},
            {rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK, MTLPixelFormatASTC_8x8_LDR},
            {rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK, MTLPixelFormatASTC_8x8_sRGB},
            {rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK, MTLPixelFormatASTC_10x10_LDR},
            {rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK, MTLPixelFormatASTC_10x10_sRGB},
            {rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK, MTLPixelFormatASTC_12x12_LDR},
            {rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK, MTLPixelFormatASTC_12x12_sRGB},
    };

    std::unordered_map<rhi::Format, MTLVertexFormat> VERTEX_FORMAT_TABLE = {
        {rhi::Format::UNDEFINED, MTLVertexFormatInvalid},
        {rhi::Format::F_R32, MTLVertexFormatFloat},
        {rhi::Format::F_RG32, MTLVertexFormatFloat2},
        {rhi::Format::F_RGB32, MTLVertexFormatFloat3},
        {rhi::Format::F_RGBA32, MTLVertexFormatFloat4},
        {rhi::Format::F_R8, MTLVertexFormatChar},
        {rhi::Format::F_RG8, MTLVertexFormatChar2},
        {rhi::Format::F_RGB8, MTLVertexFormatChar3},
        {rhi::Format::F_RGBA8, MTLVertexFormatChar4},
    };

    std::unordered_map<rhi::WrapMode, MTLSamplerAddressMode> ADDRESS_MODE_TABLE = {
        {rhi::WrapMode::REPEAT              , MTLSamplerAddressModeRepeat },
        {rhi::WrapMode::MIRRORED_REPEAT     , MTLSamplerAddressModeMirrorRepeat },
        {rhi::WrapMode::CLAMP_TO_EDGE       , MTLSamplerAddressModeClampToEdge },
        {rhi::WrapMode::CLAMP_TO_BORDER     , MTLSamplerAddressModeClampToBorderColor },
        {rhi::WrapMode::MIRROR_CLAMP_TO_EDGE, MTLSamplerAddressModeMirrorClampToEdge},
    };

    std::unordered_map<rhi::CompareOp, MTLCompareFunction> COMPARE_FUNCTION_TABLE = {
        {rhi::CompareOp::NEVER, MTLCompareFunctionNever},
        {rhi::CompareOp::LESS, MTLCompareFunctionLess},
        {rhi::CompareOp::EQUAL, MTLCompareFunctionEqual},
        {rhi::CompareOp::LESS_OR_EQUAL, MTLCompareFunctionLessEqual},
        {rhi::CompareOp::GREATER, MTLCompareFunctionGreater},
        {rhi::CompareOp::NOT_EQUAL, MTLCompareFunctionNotEqual},
        {rhi::CompareOp::GREATER_OR_EQUAL, MTLCompareFunctionGreaterEqual},
        {rhi::CompareOp::ALWAYS, MTLCompareFunctionAlways},
    };

    std::unordered_map<rhi::StencilOp, MTLStencilOperation> STENCIL_FUNCTION_TABLE = {
        {rhi::StencilOp::KEEP, MTLStencilOperationKeep},
        {rhi::StencilOp::ZERO, MTLStencilOperationZero},
        {rhi::StencilOp::REPLACE, MTLStencilOperationReplace},
        {rhi::StencilOp::INCREMENT_AND_CLAMP, MTLStencilOperationIncrementClamp},
        {rhi::StencilOp::DECREMENT_AND_CLAMP, MTLStencilOperationDecrementClamp},
        {rhi::StencilOp::INVERT, MTLStencilOperationInvert},
        {rhi::StencilOp::INCREMENT_AND_WRAP, MTLStencilOperationIncrementWrap},
        {rhi::StencilOp::DECREMENT_AND_WRAP, MTLStencilOperationDecrementWrap},
    };

    std::unordered_map<rhi::BlendFactor, MTLBlendFactor> BLEND_FACTOR_TABLE = {
        {rhi::BlendFactor::ZERO, MTLBlendFactorZero},
        {rhi::BlendFactor::ONE, MTLBlendFactorOne},
        {rhi::BlendFactor::SRC_COLOR, MTLBlendFactorSourceColor},
        {rhi::BlendFactor::ONE_MINUS_SRC_COLOR, MTLBlendFactorOneMinusSourceColor},
        {rhi::BlendFactor::DST_COLOR, MTLBlendFactorDestinationColor},
        {rhi::BlendFactor::ONE_MINUS_DST_COLOR, MTLBlendFactorOneMinusDestinationColor},
        {rhi::BlendFactor::SRC_ALPHA, MTLBlendFactorSourceAlpha},
        {rhi::BlendFactor::ONE_MINUS_SRC_ALPHA, MTLBlendFactorOneMinusSourceAlpha},
        {rhi::BlendFactor::DST_ALPHA, MTLBlendFactorDestinationAlpha},
        {rhi::BlendFactor::ONE_MINUS_DST_ALPHA, MTLBlendFactorOneMinusDestinationAlpha},
//        {rhi::BlendFactor::CONSTANT_COLOR, MTLBlendFactor},
//        {rhi::BlendFactor::ONE_MINUS_CONSTANT_COLOR, MTLBlendFactor},
//        {rhi::BlendFactor::CONSTANT_ALPHA, MTLBlendFactor},
//        {rhi::BlendFactor::ONE_MINUS_CONSTANT_ALPHA, MTLBlendFactor},
//        {rhi::BlendFactor::SRC_ALPHA_SATURATE, MTLBlendFactor},
        {rhi::BlendFactor::SRC1_COLOR, MTLBlendFactorSource1Color},
        {rhi::BlendFactor::ONE_MINUS_SRC1_COLOR, MTLBlendFactorOneMinusSource1Color},
        {rhi::BlendFactor::SRC1_ALPHA, MTLBlendFactorSourceAlpha},
        {rhi::BlendFactor::ONE_MINUS_SRC1_ALPHA, MTLBlendFactorOneMinusSource1Alpha},
    };

    std::unordered_map<rhi::PrimitiveTopology, MTLPrimitiveTopologyClass> PRIMITIVE_TOPOLOGY_TABLE = {
        {rhi::PrimitiveTopology::POINT_LIST, MTLPrimitiveTopologyClassPoint},
        {rhi::PrimitiveTopology::LINE_LIST, MTLPrimitiveTopologyClassLine},
        {rhi::PrimitiveTopology::TRIANGLE_LIST, MTLPrimitiveTopologyClassTriangle},
    };

    std::unordered_map<rhi::PrimitiveTopology, MTLPrimitiveType> PRIMITIVE_TYPE_TABLE = {
        {rhi::PrimitiveTopology::POINT_LIST, MTLPrimitiveTypePoint},
        {rhi::PrimitiveTopology::LINE_LIST, MTLPrimitiveTypeLine},
        {rhi::PrimitiveTopology::LINE_STRIP, MTLPrimitiveTypeLineStrip},
        {rhi::PrimitiveTopology::TRIANGLE_LIST, MTLPrimitiveTypeTriangle},
        {rhi::PrimitiveTopology::TRIANGLE_STRIP, MTLPrimitiveTypeTriangleStrip},
    };

    static const MTLCullMode CULL_MODE_TABLE[] = {
        MTLCullModeNone,
        MTLCullModeFront,
        MTLCullModeBack
    };

    static const MTLBlendOperation BLEND_OP_TABLE[] = {
        MTLBlendOperationAdd,
        MTLBlendOperationSubtract
    };

    static const MTLLoadAction LOAD_OP_MAP[] = {
        MTLLoadActionDontCare,
        MTLLoadActionLoad,
        MTLLoadActionClear
    };

    static const MTLStoreAction STORE_OP_MAP[] = {
        MTLStoreActionDontCare,
        MTLStoreActionStore
    };

    MTLPixelFormat FromRHI(rhi::PixelFormat format)
    {
        auto iter = PIXEL_FORMAT_TABLE.find(format);
        return iter == PIXEL_FORMAT_TABLE.end() ? MTLPixelFormatInvalid : iter->second;
    }

    MTLVertexFormat FromRHI(rhi::Format format)
    {
        auto iter = VERTEX_FORMAT_TABLE.find(format);
        return iter == VERTEX_FORMAT_TABLE.end() ? MTLVertexFormatInvalid : iter->second;
    }

    MTLStorageMode FromRHI(rhi::ImageUsageFlags usage, rhi::MemoryType memory)
    {
        if ((usage & rhi::ImageUsageFlagBit::TRANSIENT) && memory == rhi::MemoryType::GPU_ONLY) {
            return MTLStorageModeMemoryless;
        }

        if (memory == rhi::MemoryType::GPU_ONLY) {
            return MTLStorageModePrivate;
        }

        if (memory == rhi::MemoryType::CPU_TO_GPU) {
            return MTLStorageModeShared;
        }

        return MTLStorageModeManaged;
    }

    MTLResourceOptions FromRHI(rhi::BufferUsageFlags usage, rhi::MemoryType memory)
    {
        if (memory == rhi::MemoryType::GPU_ONLY) {
            return MTLResourceStorageModePrivate;
        }

        if (memory == rhi::MemoryType::CPU_TO_GPU) {
            return MTLResourceStorageModeShared;
        }

        return MTLResourceStorageModeManaged;
    }

    MTLSamplerAddressMode FromRHI(rhi::WrapMode mode)
    {
        auto iter = ADDRESS_MODE_TABLE.find(mode);
        return iter == ADDRESS_MODE_TABLE.end() ? MTLSamplerAddressModeClampToEdge : iter->second;
    }

    MTLSamplerMinMagFilter FromRHI(rhi::Filter filter)
    {
        return filter == rhi::Filter::LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    }

    MTLSamplerMipFilter FromRHI(rhi::MipFilter filter)
    {
        return filter == rhi::MipFilter::LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    }

    MTLCompareFunction FromRHI(rhi::CompareOp op)
    {
        auto iter = COMPARE_FUNCTION_TABLE.find(op);
        return iter == COMPARE_FUNCTION_TABLE.end() ? MTLCompareFunctionAlways : iter->second;
    }

    MTLStencilOperation FromRHI(rhi::StencilOp op)
    {
        auto iter = STENCIL_FUNCTION_TABLE.find(op);
        return iter == STENCIL_FUNCTION_TABLE.end() ? MTLStencilOperationKeep: iter->second;
    }

    MTLWinding FromRHI(rhi::FrontFace mode)
    {
        return mode == rhi::FrontFace::CCW ? MTLWindingCounterClockwise : MTLWindingClockwise;
    }

    MTLCullMode FromRHI(rhi::CullingModeFlags mode)
    {
        return CULL_MODE_TABLE[mode.value];
    }

    MTLTriangleFillMode FromRHI(rhi::PolygonMode mode)
    {
        return mode == rhi::PolygonMode::FILL ? MTLTriangleFillModeFill : MTLTriangleFillModeLines;
    }

    MTLColorWriteMask FromRHI(uint32_t writeMask)
    {
        MTLColorWriteMask mask = MTLColorWriteMaskNone;
        if (writeMask & 0x08) {
            mask |= MTLColorWriteMaskRed;
        }
        if (writeMask & 0x04) {
            mask |= MTLColorWriteMaskGreen;
        }
        if (writeMask & 0x02) {
            mask |= MTLColorWriteMaskBlue;
        }
        if (writeMask & 0x01) {
            mask |= MTLColorWriteMaskAlpha;
        }
        return mask;
    }

    MTLBlendOperation FromRHI(rhi::BlendOp op)
    {
        return BLEND_OP_TABLE[static_cast<uint32_t>(op)];
    }

    MTLBlendFactor FromRHI(rhi::BlendFactor factor)
    {
        auto iter = BLEND_FACTOR_TABLE.find(factor);
        SKY_ASSERT(iter != BLEND_FACTOR_TABLE.end() && "blend factor not supported.");
        return iter->second;
    }

    MTLPrimitiveType FromRHI(rhi::PrimitiveTopology topology)
    {
        auto iter = PRIMITIVE_TYPE_TABLE.find(topology);
        return iter != PRIMITIVE_TYPE_TABLE.end() ? iter->second : MTLPrimitiveTypeTriangle;
    }

    MTLLoadAction FromRHI(rhi::LoadOp op)
    {
        return LOAD_OP_MAP[static_cast<uint32_t>(op)];
    }

    MTLStoreAction FromRHI(rhi::StoreOp op)
    {
        return STORE_OP_MAP[static_cast<uint32_t>(op)];
    }


}
