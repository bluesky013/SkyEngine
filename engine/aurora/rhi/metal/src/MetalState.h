//
// aurora <-> Metal pipeline / encoder state conversions.
//

#pragma once

#import <Metal/Metal.h>

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    inline MTLLoadAction ToMetalLoadAction(LoadOp op)
    {
        switch (op) {
        case LoadOp::LOAD: return MTLLoadActionLoad;
        case LoadOp::CLEAR: return MTLLoadActionClear;
        default: return MTLLoadActionDontCare;
        }
    }

    inline MTLStoreAction ToMetalStoreAction(StoreOp op)
    {
        return op == StoreOp::STORE ? MTLStoreActionStore : MTLStoreActionDontCare;
    }

    inline MTLIndexType ToMetalIndexType(IndexType type)
    {
        return type == IndexType::U32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    }

    inline MTLSamplerAddressMode ToMetalAddressMode(WrapMode mode)
    {
        switch (mode) {
        case WrapMode::REPEAT: return MTLSamplerAddressModeRepeat;
        case WrapMode::MIRRORED_REPEAT: return MTLSamplerAddressModeMirrorRepeat;
        case WrapMode::CLAMP_TO_BORDER: return MTLSamplerAddressModeClampToBorderColor;
        case WrapMode::MIRROR_CLAMP_TO_EDGE: return MTLSamplerAddressModeMirrorClampToEdge;
        case WrapMode::CLAMP_TO_EDGE:
        default:
            return MTLSamplerAddressModeClampToEdge;
        }
    }

    inline MTLSamplerMinMagFilter ToMetalFilter(Filter filter)
    {
        return filter == Filter::LINEAR ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
    }

    inline MTLSamplerMipFilter ToMetalMipFilter(MipFilter filter)
    {
        return filter == MipFilter::LINEAR ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
    }

    inline MTLCompareFunction ToMetalCompare(CompareOp compareOp)
    {
        switch (compareOp) {
        case CompareOp::NEVER: return MTLCompareFunctionNever;
        case CompareOp::LESS: return MTLCompareFunctionLess;
        case CompareOp::EQUAL: return MTLCompareFunctionEqual;
        case CompareOp::LESS_OR_EQUAL: return MTLCompareFunctionLessEqual;
        case CompareOp::GREATER: return MTLCompareFunctionGreater;
        case CompareOp::NOT_EQUAL: return MTLCompareFunctionNotEqual;
        case CompareOp::GREATER_OR_EQUAL: return MTLCompareFunctionGreaterEqual;
        case CompareOp::ALWAYS:
        default:
            return MTLCompareFunctionAlways;
        }
    }

    inline MTLStencilOperation ToMetalStencilOp(StencilOp op)
    {
        switch (op) {
        case StencilOp::ZERO: return MTLStencilOperationZero;
        case StencilOp::REPLACE: return MTLStencilOperationReplace;
        case StencilOp::INCREMENT_AND_CLAMP: return MTLStencilOperationIncrementClamp;
        case StencilOp::DECREMENT_AND_CLAMP: return MTLStencilOperationDecrementClamp;
        case StencilOp::INVERT: return MTLStencilOperationInvert;
        case StencilOp::INCREMENT_AND_WRAP: return MTLStencilOperationIncrementWrap;
        case StencilOp::DECREMENT_AND_WRAP: return MTLStencilOperationDecrementWrap;
        case StencilOp::KEEP:
        default:
            return MTLStencilOperationKeep;
        }
    }

    inline MTLBlendFactor ToMetalBlendFactor(BlendFactor factor)
    {
        switch (factor) {
        case BlendFactor::ZERO: return MTLBlendFactorZero;
        case BlendFactor::ONE: return MTLBlendFactorOne;
        case BlendFactor::SRC_COLOR: return MTLBlendFactorSourceColor;
        case BlendFactor::ONE_MINUS_SRC_COLOR: return MTLBlendFactorOneMinusSourceColor;
        case BlendFactor::DST_COLOR: return MTLBlendFactorDestinationColor;
        case BlendFactor::ONE_MINUS_DST_COLOR: return MTLBlendFactorOneMinusDestinationColor;
        case BlendFactor::SRC_ALPHA: return MTLBlendFactorSourceAlpha;
        case BlendFactor::ONE_MINUS_SRC_ALPHA: return MTLBlendFactorOneMinusSourceAlpha;
        case BlendFactor::DST_ALPHA: return MTLBlendFactorDestinationAlpha;
        case BlendFactor::ONE_MINUS_DST_ALPHA: return MTLBlendFactorOneMinusDestinationAlpha;
        case BlendFactor::SRC1_COLOR: return MTLBlendFactorSource1Color;
        case BlendFactor::ONE_MINUS_SRC1_COLOR: return MTLBlendFactorOneMinusSource1Color;
        case BlendFactor::SRC1_ALPHA: return MTLBlendFactorSource1Alpha;
        case BlendFactor::ONE_MINUS_SRC1_ALPHA: return MTLBlendFactorOneMinusSource1Alpha;
        default:
            return MTLBlendFactorOne;
        }
    }

    inline MTLBlendOperation ToMetalBlendOp(BlendOp op)
    {
        return op == BlendOp::SUBTRACT ? MTLBlendOperationSubtract : MTLBlendOperationAdd;
    }

    inline MTLWinding ToMetalWinding(FrontFace face)
    {
        return face == FrontFace::CCW ? MTLWindingCounterClockwise : MTLWindingClockwise;
    }

    inline MTLCullMode ToMetalCullMode(CullingModeFlags mode)
    {
        if (mode == CullModeFlagBits::FRONT) {
            return MTLCullModeFront;
        }
        if (mode == CullModeFlagBits::BACK) {
            return MTLCullModeBack;
        }
        return MTLCullModeNone;
    }

    inline MTLTriangleFillMode ToMetalFillMode(PolygonMode mode)
    {
        return mode == PolygonMode::FILL ? MTLTriangleFillModeFill : MTLTriangleFillModeLines;
    }

    inline MTLPrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology)
    {
        switch (topology) {
        case PrimitiveTopology::POINT_LIST: return MTLPrimitiveTypePoint;
        case PrimitiveTopology::LINE_LIST: return MTLPrimitiveTypeLine;
        case PrimitiveTopology::LINE_STRIP: return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::TRIANGLE_STRIP: return MTLPrimitiveTypeTriangleStrip;
        case PrimitiveTopology::TRIANGLE_LIST:
        default:
            return MTLPrimitiveTypeTriangle;
        }
    }

    inline MTLPrimitiveTopologyClass ToMetalPrimitiveTopology(PrimitiveTopology topology)
    {
        switch (topology) {
        case PrimitiveTopology::POINT_LIST: return MTLPrimitiveTopologyClassPoint;
        case PrimitiveTopology::LINE_LIST:
        case PrimitiveTopology::LINE_STRIP:
            return MTLPrimitiveTopologyClassLine;
        case PrimitiveTopology::TRIANGLE_LIST:
        case PrimitiveTopology::TRIANGLE_STRIP:
        case PrimitiveTopology::TRIANGLE_FAN:
        default:
            return MTLPrimitiveTopologyClassTriangle;
        }
    }

    inline MTLColorWriteMask ToMetalWriteMask(uint8_t writeMask)
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

    inline NSString *ToMetalEntryPoint(ShaderStageFlagBit stage)
    {
        switch (stage) {
        case ShaderStageFlagBit::FS: return @"FSMain";
        case ShaderStageFlagBit::CS: return @"CSMain";
        case ShaderStageFlagBit::VS:
        default:
            return @"VSMain";
        }
    }

} // namespace sky::aurora
