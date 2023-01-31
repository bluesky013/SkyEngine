//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/Conversion.h>
#include <unordered_map>

namespace sky::gles {

    const std::unordered_map<rhi::PixelFormat, FormatType> FORMAT_MAP = {
        {rhi::PixelFormat::UNDEFINED,                 {0,               0,       0}},
        {rhi::PixelFormat::RGBA8_UNORM,               {GL_RGBA8,        GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::RGBA8_SRGB,                {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BGRA8_UNORM,               {GL_RGBA8,        GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::BGRA8_SRGB,                {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::D32,                       {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT}},
        {rhi::PixelFormat::D24_S8,                    {GL_DEPTH24_STENCIL8,   GL_DEPTH_STENCIL,   GL_UNSIGNED_INT_24_8}},
        {rhi::PixelFormat::D32_S8,                    {GL_DEPTH32F_STENCIL8,  GL_DEPTH_STENCIL,   GL_FLOAT_32_UNSIGNED_INT_24_8_REV}},

        {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK,       {GL_COMPRESSED_RGB_S3TC_DXT1_EXT,        GL_RGB,  GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK,        {GL_COMPRESSED_SRGB_S3TC_DXT1_EXT,       GL_RGB,  GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK,      {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,       GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK,       {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::BC2_UNORM_BLOCK,           {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,       GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC2_SRGB_BLOCK,            {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::BC3_UNORM_BLOCK,           {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,       GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC3_SRGB_BLOCK,            {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::BC4_UNORM_BLOCK,           {GL_COMPRESSED_RED_RGTC1_EXT,        GL_RED, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC4_SNORM_BLOCK,           {GL_COMPRESSED_SIGNED_RED_RGTC1_EXT, GL_RED, GL_BYTE}},

        {rhi::PixelFormat::BC5_UNORM_BLOCK,           {GL_COMPRESSED_RED_GREEN_RGTC2_EXT,        GL_RG, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC5_SNORM_BLOCK,           {GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT, GL_RG, GL_BYTE}},

        {rhi::PixelFormat::BC6H_UFLOAT_BLOCK,         {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT, GL_RGB, GL_FLOAT}},
        {rhi::PixelFormat::BC6H_SFLOAT_BLOCK,         {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT,   GL_RGB, GL_FLOAT}},

        {rhi::PixelFormat::BC7_UNORM_BLOCK,           {GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,       GL_RGBA, GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::BC7_SRGB_BLOCK,            {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT, GL_RGBA, GL_UNSIGNED_BYTE}},

        {rhi::PixelFormat::ETC2_R8G8B8_UNORM_BLOCK,   {GL_COMPRESSED_RGB8_ETC2}},
        {rhi::PixelFormat::ETC2_R8G8B8_SRGB_BLOCK,    {GL_COMPRESSED_SRGB8_ETC2, }},
        {rhi::PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, {GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2}},
        {rhi::PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK,  {GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2}},
        {rhi::PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, {GL_COMPRESSED_RGBA8_ETC2_EAC}},
        {rhi::PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK,  {GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC}},

        {rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK,      {GL_COMPRESSED_RGBA_ASTC_4x4_KHR}},
        {rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK,       {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR}},
        {rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK,      {GL_COMPRESSED_RGBA_ASTC_8x8_KHR}},
        {rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK,       {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR}},
        {rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK,    {GL_COMPRESSED_RGBA_ASTC_10x10_KHR}},
        {rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK,     {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR}},
        {rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK,    {GL_COMPRESSED_RGBA_ASTC_12x12_KHR}},
        {rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK,     {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR}},
    };

    const FormatType &GetFormatInfo(rhi::PixelFormat format)
    {
        return FORMAT_MAP.find(format)->second;
    }

}