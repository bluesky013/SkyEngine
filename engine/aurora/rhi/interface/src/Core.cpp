//
// Created on 2026/04/06.
//

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    // Indexed by PixelFormat enum value.
    // {components, blockSize, blockWidth, blockHeight, isCompressed, hasDepth, hasStencil}
    static const ImageFormatInfo FORMAT_INFO_TABLE[] = {
        // UNDEFINED
        {0, 0, 1, 1, false, false, false},
        // R8_UINT
        {1, 1, 1, 1, false, false, false},
        // R8_UNORM
        {1, 1, 1, 1, false, false, false},
        // R8_SRGB
        {1, 1, 1, 1, false, false, false},
        // RGBA8_UNORM
        {4, 4, 1, 1, false, false, false},
        // RGBA8_SRGB
        {4, 4, 1, 1, false, false, false},
        // BGRA8_UNORM
        {4, 4, 1, 1, false, false, false},
        // BGRA8_SRGB
        {4, 4, 1, 1, false, false, false},
        // R16_UNORM
        {1, 2, 1, 1, false, false, false},
        // RG16_UNORM
        {2, 4, 1, 1, false, false, false},
        // RGBA16_UNORM
        {4, 8, 1, 1, false, false, false},
        // R16_SFLOAT
        {1, 2, 1, 1, false, false, false},
        // RG16_SFLOAT
        {2, 4, 1, 1, false, false, false},
        // RGBA16_SFLOAT
        {4, 8, 1, 1, false, false, false},
        // R32_SFLOAT
        {1, 4, 1, 1, false, false, false},
        // RG32_SFLOAT
        {2, 8, 1, 1, false, false, false},
        // RGB32_SFLOAT
        {3, 12, 1, 1, false, false, false},
        // RGBA32_SFLOAT
        {4, 16, 1, 1, false, false, false},
        // R32_UINT
        {1, 4, 1, 1, false, false, false},
        // RG32_UINT
        {2, 8, 1, 1, false, false, false},
        // RGB32_UINT
        {3, 12, 1, 1, false, false, false},
        // RGBA32_UINT
        {4, 16, 1, 1, false, false, false},
        // D32
        {1, 4, 1, 1, false, true, false},
        // D24_S8
        {2, 4, 1, 1, false, true, true},
        // D32_S8
        {2, 5, 1, 1, false, true, true},
        // BC1_RGB_UNORM_BLOCK
        {3, 8, 4, 4, true, false, false},
        // BC1_RGB_SRGB_BLOCK
        {3, 8, 4, 4, true, false, false},
        // BC1_RGBA_UNORM_BLOCK
        {4, 8, 4, 4, true, false, false},
        // BC1_RGBA_SRGB_BLOCK
        {4, 8, 4, 4, true, false, false},
        // BC2_UNORM_BLOCK
        {4, 16, 4, 4, true, false, false},
        // BC2_SRGB_BLOCK
        {4, 16, 4, 4, true, false, false},
        // BC3_UNORM_BLOCK
        {4, 16, 4, 4, true, false, false},
        // BC3_SRGB_BLOCK
        {4, 16, 4, 4, true, false, false},
        // BC4_UNORM_BLOCK
        {1, 8, 4, 4, true, false, false},
        // BC4_SNORM_BLOCK
        {1, 8, 4, 4, true, false, false},
        // BC5_UNORM_BLOCK
        {2, 16, 4, 4, true, false, false},
        // BC5_SNORM_BLOCK
        {2, 16, 4, 4, true, false, false},
        // BC6H_UFLOAT_BLOCK
        {3, 16, 4, 4, true, false, false},
        // BC6H_SFLOAT_BLOCK
        {3, 16, 4, 4, true, false, false},
        // BC7_UNORM_BLOCK
        {4, 16, 4, 4, true, false, false},
        // BC7_SRGB_BLOCK
        {4, 16, 4, 4, true, false, false},
        // ETC2_R8G8B8_UNORM_BLOCK
        {3, 8, 4, 4, true, false, false},
        // ETC2_R8G8B8_SRGB_BLOCK
        {3, 8, 4, 4, true, false, false},
        // ETC2_R8G8B8A1_UNORM_BLOCK
        {4, 8, 4, 4, true, false, false},
        // ETC2_R8G8B8A1_SRGB_BLOCK
        {4, 8, 4, 4, true, false, false},
        // ETC2_R8G8B8A8_UNORM_BLOCK
        {4, 16, 4, 4, true, false, false},
        // ETC2_R8G8B8A8_SRGB_BLOCK
        {4, 16, 4, 4, true, false, false},
        // ASTC_4x4_UNORM_BLOCK
        {4, 16, 4, 4, true, false, false},
        // ASTC_4x4_SRGB_BLOCK
        {4, 16, 4, 4, true, false, false},
        // ASTC_8x8_UNORM_BLOCK
        {4, 16, 8, 8, true, false, false},
        // ASTC_8x8_SRGB_BLOCK
        {4, 16, 8, 8, true, false, false},
        // ASTC_10x10_UNORM_BLOCK
        {4, 16, 10, 10, true, false, false},
        // ASTC_10x10_SRGB_BLOCK
        {4, 16, 10, 10, true, false, false},
        // ASTC_12x12_UNORM_BLOCK
        {4, 16, 12, 12, true, false, false},
        // ASTC_12x12_SRGB_BLOCK
        {4, 16, 12, 12, true, false, false},
    };

    static_assert(sizeof(FORMAT_INFO_TABLE) / sizeof(FORMAT_INFO_TABLE[0]) == static_cast<uint32_t>(PixelFormat::MAX),
                  "FORMAT_INFO_TABLE must cover every PixelFormat entry");

    const ImageFormatInfo &GetImageFormatInfo(PixelFormat format)
    {
        const auto idx = static_cast<uint32_t>(format);
        if (idx < static_cast<uint32_t>(PixelFormat::MAX)) {
            return FORMAT_INFO_TABLE[idx];
        }
        return FORMAT_INFO_TABLE[0]; // UNDEFINED
    }

} // namespace sky::aurora
