//
// aurora <-> Metal format / image / buffer descriptors.
//

#pragma once

#import <Metal/Metal.h>

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    inline MTLPixelFormat ToMetalPixelFormat(PixelFormat format)
    {
        switch (format) {
        case PixelFormat::R8_UINT: return MTLPixelFormatR8Uint;
        case PixelFormat::R8_UNORM: return MTLPixelFormatR8Unorm;
        case PixelFormat::R8_SRGB: return MTLPixelFormatR8Unorm_sRGB;
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
        // BCn: desktop GPUs only (macOS); unsupported on Apple-family GPUs
        case PixelFormat::BC1_RGB_UNORM_BLOCK: return MTLPixelFormatBC1_RGBA;
        case PixelFormat::BC1_RGB_SRGB_BLOCK: return MTLPixelFormatBC1_RGBA_sRGB;
        case PixelFormat::BC1_RGBA_UNORM_BLOCK: return MTLPixelFormatBC1_RGBA;
        case PixelFormat::BC1_RGBA_SRGB_BLOCK: return MTLPixelFormatBC1_RGBA_sRGB;
        case PixelFormat::BC2_UNORM_BLOCK: return MTLPixelFormatBC2_RGBA;
        case PixelFormat::BC2_SRGB_BLOCK: return MTLPixelFormatBC2_RGBA_sRGB;
        case PixelFormat::BC3_UNORM_BLOCK: return MTLPixelFormatBC3_RGBA;
        case PixelFormat::BC3_SRGB_BLOCK: return MTLPixelFormatBC3_RGBA_sRGB;
        case PixelFormat::BC4_UNORM_BLOCK: return MTLPixelFormatBC4_RUnorm;
        case PixelFormat::BC4_SNORM_BLOCK: return MTLPixelFormatBC4_RSnorm;
        case PixelFormat::BC5_UNORM_BLOCK: return MTLPixelFormatBC5_RGUnorm;
        case PixelFormat::BC5_SNORM_BLOCK: return MTLPixelFormatBC5_RGSnorm;
        case PixelFormat::BC6H_UFLOAT_BLOCK: return MTLPixelFormatBC6H_RGBUfloat;
        case PixelFormat::BC6H_SFLOAT_BLOCK: return MTLPixelFormatBC6H_RGBFloat;
        case PixelFormat::BC7_UNORM_BLOCK: return MTLPixelFormatBC7_RGBAUnorm;
        case PixelFormat::BC7_SRGB_BLOCK: return MTLPixelFormatBC7_RGBAUnorm_sRGB;
        // ASTC: Apple-family GPUs (A8+/M1+)
        case PixelFormat::ASTC_4x4_UNORM_BLOCK: return MTLPixelFormatASTC_4x4_LDR;
        case PixelFormat::ASTC_4x4_SRGB_BLOCK: return MTLPixelFormatASTC_4x4_sRGB;
        case PixelFormat::ASTC_8x8_UNORM_BLOCK: return MTLPixelFormatASTC_8x8_LDR;
        case PixelFormat::ASTC_8x8_SRGB_BLOCK: return MTLPixelFormatASTC_8x8_sRGB;
        case PixelFormat::ASTC_10x10_UNORM_BLOCK: return MTLPixelFormatASTC_10x10_LDR;
        case PixelFormat::ASTC_10x10_SRGB_BLOCK: return MTLPixelFormatASTC_10x10_sRGB;
        case PixelFormat::ASTC_12x12_UNORM_BLOCK: return MTLPixelFormatASTC_12x12_LDR;
        case PixelFormat::ASTC_12x12_SRGB_BLOCK: return MTLPixelFormatASTC_12x12_sRGB;
        case PixelFormat::ASTC_6x6_UNORM_BLOCK: return MTLPixelFormatASTC_6x6_LDR;
        case PixelFormat::ASTC_6x6_SRGB_BLOCK: return MTLPixelFormatASTC_6x6_sRGB;
        // ETC2 has no Metal equivalent
        default: return MTLPixelFormatInvalid;
        }
    }

    // aurora vertex attribute Format -> MTLVertexFormat (mirrors the Vulkan
    // FORMAT_TABLE semantics: F_* = float/unorm, U_* = uint)
    inline MTLVertexFormat ToMetalVertexFormat(Format format)
    {
        switch (format) {
        case Format::F_R32: return MTLVertexFormatFloat;
        case Format::F_RG32: return MTLVertexFormatFloat2;
        case Format::F_RGB32: return MTLVertexFormatFloat3;
        case Format::F_RGBA32: return MTLVertexFormatFloat4;
        case Format::F_R8: return MTLVertexFormatUCharNormalized;
        case Format::F_RG8: return MTLVertexFormatUChar2Normalized;
        case Format::F_RGB8: return MTLVertexFormatUChar3Normalized;
        case Format::F_RGBA8: return MTLVertexFormatUChar4Normalized;
        case Format::U_R8: return MTLVertexFormatUChar;
        case Format::U_RG8: return MTLVertexFormatUChar2;
        case Format::U_RGB8: return MTLVertexFormatUChar3;
        case Format::U_RGBA8: return MTLVertexFormatUChar4;
        case Format::U_R16: return MTLVertexFormatUShort;
        case Format::U_RG16: return MTLVertexFormatUShort2;
        case Format::U_RGB16: return MTLVertexFormatUShort3;
        case Format::U_RGBA16: return MTLVertexFormatUShort4;
        case Format::U_R32: return MTLVertexFormatUInt;
        case Format::U_RG32: return MTLVertexFormatUInt2;
        case Format::U_RGB32: return MTLVertexFormatUInt3;
        case Format::U_RGBA32: return MTLVertexFormatUInt4;
        default: return MTLVertexFormatInvalid;
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
        case ImageType::IMAGE_1D: return desc.arrayLayers > 1 ? MTLTextureType1DArray : MTLTextureType1D;
        case ImageType::IMAGE_3D: return MTLTextureType3D;
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

} // namespace sky::aurora
