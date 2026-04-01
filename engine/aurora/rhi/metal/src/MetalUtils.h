//
// Created on 2026/04/02.
//

#pragma once

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    inline MTLPixelFormat ToMetalPixelFormat(PixelFormat format)
    {
        switch (format) {
        case PixelFormat::R8_UINT: return MTLPixelFormatR8Uint;
        case PixelFormat::R8_UNORM: return MTLPixelFormatR8Unorm;
        case PixelFormat::RGBA8_UNORM: return MTLPixelFormatRGBA8Unorm;
        case PixelFormat::RGBA8_SRGB: return MTLPixelFormatRGBA8Unorm_sRGB;
        case PixelFormat::BGRA8_UNORM: return MTLPixelFormatBGRA8Unorm;
        case PixelFormat::BGRA8_SRGB: return MTLPixelFormatBGRA8Unorm_sRGB;
        case PixelFormat::R16_UNORM: return MTLPixelFormatR16Unorm;
        case PixelFormat::RG16_UNORM: return MTLPixelFormatRG16Unorm;
        case PixelFormat::RGBA16_UNORM: return MTLPixelFormatRGBA16Unorm;
        case PixelFormat::R16_SFLOAT: return MTLPixelFormatR16Float;
        case PixelFormat::RG16_SFLOAT: return MTLPixelFormatRG16Float;
        case PixelFormat::RGBA16_SFLOAT: return MTLPixelFormatRGBA16Float;
        case PixelFormat::R32_SFLOAT: return MTLPixelFormatR32Float;
        case PixelFormat::RG32_SFLOAT: return MTLPixelFormatRG32Float;
        case PixelFormat::RGBA32_SFLOAT: return MTLPixelFormatRGBA32Float;
        case PixelFormat::R32_UINT: return MTLPixelFormatR32Uint;
        case PixelFormat::RG32_UINT: return MTLPixelFormatRG32Uint;
        case PixelFormat::RGBA32_UINT: return MTLPixelFormatRGBA32Uint;
        case PixelFormat::D32: return MTLPixelFormatDepth32Float;
        case PixelFormat::D24_S8: return MTLPixelFormatDepth32Float_Stencil8;
        case PixelFormat::D32_S8: return MTLPixelFormatDepth32Float_Stencil8;
        default: return MTLPixelFormatInvalid;
        }
    }

    inline NSUInteger ToMetalSampleCount(SampleCount sampleCount)
    {
        return static_cast<NSUInteger>(sampleCount);
    }

    inline MTLTextureType ToMetalTextureType(const Image::Descriptor &desc)
    {
        if ((desc.viewUsage & ImageViewUsageFlagBit::CUBE_MAP_COMPATIBLE) && desc.arrayLayers >= 6) {
            return desc.arrayLayers > 6 ? MTLTextureTypeCubeArray : MTLTextureTypeCube;
        }

        switch (desc.imageType) {
        case ImageType::IMAGE_1D:
            return desc.arrayLayers > 1 ? MTLTextureType1DArray : MTLTextureType1D;
        case ImageType::IMAGE_3D:
            return MTLTextureType3D;
        case ImageType::IMAGE_2D:
        default:
            if (desc.samples != SampleCount::X1) {
                return desc.arrayLayers > 1 ? MTLTextureType2DMultisampleArray : MTLTextureType2DMultisample;
            }
            return desc.arrayLayers > 1 ? MTLTextureType2DArray : MTLTextureType2D;
        }
    }

    inline MTLTextureUsage ToMetalTextureUsage(ImageUsageFlags usage)
    {
        MTLTextureUsage result = MTLTextureUsageUnknown;
        if (usage & ImageUsageFlagBit::SAMPLED) {
            result |= MTLTextureUsageShaderRead;
        }
        if (usage & ImageUsageFlagBit::STORAGE) {
            result |= MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        }
        if (usage & ImageUsageFlagBit::RENDER_TARGET) {
            result |= MTLTextureUsageRenderTarget;
        }
        if (usage & ImageUsageFlagBit::DEPTH_STENCIL) {
            result |= MTLTextureUsageRenderTarget;
        }
        return result;
    }

    inline MTLStorageMode ToMetalStorageMode(ImageUsageFlags usage, MemoryType memory)
    {
        if ((usage & ImageUsageFlagBit::TRANSIENT) && memory == MemoryType::GPU_ONLY) {
            return MTLStorageModeMemoryless;
        }
        if (memory == MemoryType::GPU_ONLY) {
            return MTLStorageModePrivate;
        }
        return MTLStorageModeShared;
    }

    inline MTLResourceOptions ToMetalBufferOptions(BufferUsageFlags usage, MemoryType memory)
    {
        (void)usage;
        if (memory == MemoryType::GPU_ONLY) {
            return MTLResourceStorageModePrivate;
        }
        return MTLResourceStorageModeShared;
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
        default: return MTLBlendFactorOne;
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