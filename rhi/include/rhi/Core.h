//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <cstdint>
#include <memory>
#include <core/template/Flags.h>

namespace sky::rhi {

#define ENABLE_FLAG_BIT_OPERATOR(Type) \
    constexpr Flags<Type> operator&(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) & rhs; \
    } \
    constexpr Flags<Type> operator|(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) | rhs; \
    } \
    constexpr Flags<Type> operator^(Type const& lhs, Type const& rhs) noexcept \
    { \
        return Flags<Type>(lhs) ^ rhs; \
    }


    enum class PixelFormat : uint32_t {
        UNDEFINED = 0,
        RGBA8_UNORM,
        RGBA8_SRGB,
        BGRA8_UNORM,
        BGRA8_SRGB,
        D32,
        D24_S8,
        D32_S8,
        BC1_RGB_UNORM_BLOCK,
        BC1_RGB_SRGB_BLOCK,
        BC1_RGBA_UNORM_BLOCK,
        BC1_RGBA_SRGB_BLOCK,
        BC2_UNORM_BLOCK,
        BC2_SRGB_BLOCK,
        BC3_UNORM_BLOCK,
        BC3_SRGB_BLOCK,
        BC4_UNORM_BLOCK,
        BC4_SNORM_BLOCK,
        BC5_UNORM_BLOCK,
        BC5_SNORM_BLOCK,
        BC6H_UFLOAT_BLOCK,
        BC6H_SFLOAT_BLOCK,
        BC7_UNORM_BLOCK,
        BC7_SRGB_BLOCK,
        ETC2_R8G8B8_UNORM_BLOCK,
        ETC2_R8G8B8_SRGB_BLOCK,
        ETC2_R8G8B8A1_UNORM_BLOCK,
        ETC2_R8G8B8A1_SRGB_BLOCK,
        ETC2_R8G8B8A8_UNORM_BLOCK,
        ETC2_R8G8B8A8_SRGB_BLOCK,
        ASTC_4x4_UNORM_BLOCK,
        ASTC_4x4_SRGB_BLOCK,
        ASTC_8x8_UNORM_BLOCK,
        ASTC_8x8_SRGB_BLOCK,
        ASTC_10x10_UNORM_BLOCK,
        ASTC_10x10_SRGB_BLOCK,
        ASTC_12x12_UNORM_BLOCK,
        ASTC_12x12_SRGB_BLOCK,
    };

    enum class ImageType : uint32_t {
        IMAGE_2D,
        IMAGE_3D
    };

    enum class ImageViewType : uint32_t {
        VIEW_2D,
        VIEW_3D,
        VIEW_CUBE
    };

    enum class MemoryType : uint32_t {
        GPU_ONLY,
        CPU_ONLY,
        CPU_TO_GPU,
        GPU_TO_CPU
    };

    enum class Filter : uint32_t {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class MipFilter : uint32_t {
        NEAREST = 0,
        LINEAR = 1,
    };

    enum class WrapMode : uint32_t {
        REPEAT               = 0,
        MIRRORED_REPEAT      = 1,
        CLAMP_TO_EDGE        = 2,
        CLAMP_TO_BORDER      = 3,
        MIRROR_CLAMP_TO_EDGE = 4,
    };

    enum class PresentMode : uint32_t {
        IMMEDIATE = 0,
        VSYNC     = 1
    };

    // flag bit
    enum class ImageUsageFlagBit : uint32_t {
        NONE             = 0x00000000,
        TRANSFER_SRC     = 0x00000001,
        TRANSFER_DST     = 0x00000002,
        SAMPLED          = 0x00000004,
        STORAGE          = 0x00000008,
        RENDER_TARGET    = 0x00000010,
        DEPTH_STENCIL    = 0x00000020,
        TRANSIENT        = 0x00000040,
        INPUT_ATTACHMENT = 0x00000080,
    };
    using ImageUsageFlags = Flags<ImageUsageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ImageUsageFlagBit)

    enum class AspectFlagBit : uint32_t {
        COLOR_BIT   = 0x00000001,
        DEPTH_BIT   = 0x00000002,
        STENCIL_BIT = 0x00000004,
    };
    using AspectFlags = Flags<AspectFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(AspectFlagBit)

    enum class BufferUsageFlagBit : uint32_t {
        NONE            = 0x00000000,
        TRANSFER_SRC    = 0x00000001,
        TRANSFER_DST    = 0x00000002,
        UNIFORM         = 0x00000004,
        STORAGE         = 0x00000008,
        VERTEX          = 0x00000010,
        INDEX           = 0x00000020,
        INDIRECT        = 0x00000040,
    };
    using BufferUsageFlags = Flags<BufferUsageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(BufferUsageFlagBit)

    enum class ShaderStageFlagBit : uint32_t {
        VS,
        FS,
        CS,
    };
    using ShaderStageFlags = Flags<ShaderStageFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(ShaderStageFlagBit)

    // structs
    struct Extent3D {
        uint32_t width;
        uint32_t height;
        uint32_t depth;
    };

    struct BufferUploadRequest {
        const uint8_t *data;
        uint64_t       offset;
        uint64_t       size;
    };

    struct ImageUploadRequest {
        const uint8_t *data;
        uint64_t       offset;
        uint32_t       mipLevel;
        uint32_t       layer;
    };

    struct ImageFormatInfo {
        uint32_t blockSize;
        uint32_t blockWidth;
        uint32_t blockHeight;
        bool isCompressed;
    };

    struct ImageSubRange {
        uint32_t baseLevel;
        uint32_t levels;
        uint32_t baseLayer;
        uint32_t layers;
    };
}