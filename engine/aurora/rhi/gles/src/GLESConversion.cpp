//
// Created on 2026/04/01.
//

#include <GLESConversion.h>
#include <cstdint>

namespace sky::aurora {

    // -----------------------------------------------------------------------
    // Extension fallbacks: if the GL header does not define them, treat as invalid (0).
    // -----------------------------------------------------------------------
#ifndef GL_R16_EXT
    #define GL_R16_EXT 0
#endif
#ifndef GL_RG16_EXT
    #define GL_RG16_EXT 0
#endif
#ifndef GL_RGBA16_EXT
    #define GL_RGBA16_EXT 0
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
    #define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0
#endif
#ifndef GL_COMPRESSED_SRGB_S3TC_DXT1_EXT
    #define GL_COMPRESSED_SRGB_S3TC_DXT1_EXT 0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT
    #define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT 0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT
    #define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT 0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
    #define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT
    #define GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT 0
#endif
#ifndef GL_COMPRESSED_RED_RGTC1_EXT
    #define GL_COMPRESSED_RED_RGTC1_EXT 0
#endif
#ifndef GL_COMPRESSED_SIGNED_RED_RGTC1_EXT
    #define GL_COMPRESSED_SIGNED_RED_RGTC1_EXT 0
#endif
#ifndef GL_COMPRESSED_RED_GREEN_RGTC2_EXT
    #define GL_COMPRESSED_RED_GREEN_RGTC2_EXT 0
#endif
#ifndef GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT
    #define GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT 0
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT
    #define GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT 0
#endif
#ifndef GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT
    #define GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT 0
#endif
#ifndef GL_COMPRESSED_RGBA_BPTC_UNORM_EXT
    #define GL_COMPRESSED_RGBA_BPTC_UNORM_EXT 0
#endif
#ifndef GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT
    #define GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT 0
#endif
#ifndef GL_MIRROR_CLAMP_TO_EDGE_EXT
    #define GL_MIRROR_CLAMP_TO_EDGE_EXT 0
#endif
#ifndef GL_SRC1_COLOR_EXT
    #define GL_SRC1_COLOR_EXT 0
#endif
#ifndef GL_ONE_MINUS_SRC1_COLOR_EXT
    #define GL_ONE_MINUS_SRC1_COLOR_EXT 0
#endif
#ifndef GL_SRC1_ALPHA_EXT
    #define GL_SRC1_ALPHA_EXT 0
#endif
#ifndef GL_ONE_MINUS_SRC1_ALPHA_EXT
    #define GL_ONE_MINUS_SRC1_ALPHA_EXT 0
#endif
#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
    #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

    // -----------------------------------------------------------------------
    // Pixel format table
    // Format entries resolve to {0,0,0,false} when the extension define is
    // absent, which signals an unsupported / invalid format.
    // -----------------------------------------------------------------------
    static const GLInternalFormat PIXEL_FORMAT_TABLE[] = {
        // UNDEFINED
        {0, 0, 0, false},
        // R8_UINT
        {GL_R8UI, GL_RED_INTEGER, GL_UNSIGNED_BYTE, false},
        // R8_UNORM
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE, false},
        // R8_SRGB  (GLES 3.2 does not have GL_SR8_EXT universally; fallback to R8)
        {GL_R8, GL_RED, GL_UNSIGNED_BYTE, false},
        // RGBA8_UNORM
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},
        // RGBA8_SRGB
        {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},
        // BGRA8_UNORM  (GLES maps BGRA to RGBA internally on most drivers)
        {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, false},
        // BGRA8_SRGB
        {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, false},
        // R16_UNORM  (EXT_texture_norm16)
        {GL_R16_EXT, GL_R16_EXT ? GL_RED : 0u, GL_R16_EXT ? GL_UNSIGNED_SHORT : 0u, false},
        // RG16_UNORM  (EXT_texture_norm16)
        {GL_RG16_EXT, GL_RG16_EXT ? GL_RG : 0u, GL_RG16_EXT ? GL_UNSIGNED_SHORT : 0u, false},
        // RGBA16_UNORM  (EXT_texture_norm16)
        {GL_RGBA16_EXT, GL_RGBA16_EXT ? GL_RGBA : 0u, GL_RGBA16_EXT ? GL_UNSIGNED_SHORT : 0u, false},
        // R16_SFLOAT
        {GL_R16F, GL_RED, GL_HALF_FLOAT, false},
        // RG16_SFLOAT
        {GL_RG16F, GL_RG, GL_HALF_FLOAT, false},
        // RGBA16_SFLOAT
        {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, false},
        // R32_SFLOAT
        {GL_R32F, GL_RED, GL_FLOAT, false},
        // RG32_SFLOAT
        {GL_RG32F, GL_RG, GL_FLOAT, false},
        // RGB32_SFLOAT
        {GL_RGB32F, GL_RGB, GL_FLOAT, false},
        // RGBA32_SFLOAT
        {GL_RGBA32F, GL_RGBA, GL_FLOAT, false},
        // R32_UINT
        {GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, false},
        // RG32_UINT
        {GL_RG32UI, GL_RG_INTEGER, GL_UNSIGNED_INT, false},
        // RGB32_UINT
        {GL_RGB32UI, GL_RGB_INTEGER, GL_UNSIGNED_INT, false},
        // RGBA32_UINT
        {GL_RGBA32UI, GL_RGBA_INTEGER, GL_UNSIGNED_INT, false},
        // D32
        {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, false},
        // D24_S8
        {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, false},
        // D32_S8
        {GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, false},
        // BC1_RGB_UNORM_BLOCK  (GL_EXT_texture_compression_s3tc; 0 = unsupported)
        {GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 0, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT != 0},
        // BC1_RGB_SRGB_BLOCK
        {GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 0, 0, GL_COMPRESSED_SRGB_S3TC_DXT1_EXT != 0},
        // BC1_RGBA_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT != 0},
        // BC1_RGBA_SRGB_BLOCK
        {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 0, 0, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT != 0},
        // BC2_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT != 0},
        // BC2_SRGB_BLOCK
        {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 0, 0, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT != 0},
        // BC3_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT != 0},
        // BC3_SRGB_BLOCK
        {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 0, 0, GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT != 0},
        // BC4_UNORM_BLOCK  (GL_EXT_texture_compression_rgtc; 0 = unsupported)
        {GL_COMPRESSED_RED_RGTC1_EXT, 0, 0, GL_COMPRESSED_RED_RGTC1_EXT != 0},
        // BC4_SNORM_BLOCK
        {GL_COMPRESSED_SIGNED_RED_RGTC1_EXT, 0, 0, GL_COMPRESSED_SIGNED_RED_RGTC1_EXT != 0},
        // BC5_UNORM_BLOCK
        {GL_COMPRESSED_RED_GREEN_RGTC2_EXT, 0, 0, GL_COMPRESSED_RED_GREEN_RGTC2_EXT != 0},
        // BC5_SNORM_BLOCK
        {GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT, 0, 0, GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT != 0},
        // BC6H_UFLOAT_BLOCK  (GL_EXT_texture_compression_bptc; 0 = unsupported)
        {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT, 0, 0, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT != 0},
        // BC6H_SFLOAT_BLOCK
        {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT, 0, 0, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT != 0},
        // BC7_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 0, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT != 0},
        // BC7_SRGB_BLOCK
        {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT, 0, 0, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT != 0},
        // ETC2_R8G8B8_UNORM_BLOCK
        {GL_COMPRESSED_RGB8_ETC2, 0, 0, true},
        // ETC2_R8G8B8_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ETC2, 0, 0, true},
        // ETC2_R8G8B8A1_UNORM_BLOCK
        {GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, true},
        // ETC2_R8G8B8A1_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 0, 0, true},
        // ETC2_R8G8B8A8_UNORM_BLOCK
        {GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, true},
        // ETC2_R8G8B8A8_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 0, 0, true},
        // ASTC_4x4_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_ASTC_4x4, 0, 0, true},
        // ASTC_4x4_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4, 0, 0, true},
        // ASTC_8x8_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_ASTC_8x8, 0, 0, true},
        // ASTC_8x8_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8, 0, 0, true},
        // ASTC_10x10_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_ASTC_10x10, 0, 0, true},
        // ASTC_10x10_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10, 0, 0, true},
        // ASTC_12x12_UNORM_BLOCK
        {GL_COMPRESSED_RGBA_ASTC_12x12, 0, 0, true},
        // ASTC_12x12_SRGB_BLOCK
        {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12, 0, 0, true},
    };
    static_assert(sizeof(PIXEL_FORMAT_TABLE) / sizeof(PIXEL_FORMAT_TABLE[0]) == static_cast<uint32_t>(PixelFormat::MAX),
                  "PIXEL_FORMAT_TABLE must match PixelFormat enum count");

    const GLInternalFormat &FromPixelFormat(PixelFormat format)
    {
        return PIXEL_FORMAT_TABLE[static_cast<uint32_t>(format)];
    }

    // -----------------------------------------------------------------------
    // Vertex format table
    // -----------------------------------------------------------------------
    static const GLVertexFormat VERTEX_FORMAT_TABLE[] = {
        // UNDEFINED
        {0, 0, GL_FALSE},
        // F_R32
        {1, GL_FLOAT, GL_FALSE},
        // F_RG32
        {2, GL_FLOAT, GL_FALSE},
        // F_RGB32
        {3, GL_FLOAT, GL_FALSE},
        // F_RGBA32
        {4, GL_FLOAT, GL_FALSE},
        // F_R8
        {1, GL_FLOAT, GL_FALSE},
        // F_RG8
        {2, GL_FLOAT, GL_FALSE},
        // F_RGB8
        {3, GL_FLOAT, GL_FALSE},
        // F_RGBA8
        {4, GL_FLOAT, GL_FALSE},
        // U_R8
        {1, GL_UNSIGNED_BYTE, GL_TRUE},
        // U_RG8
        {2, GL_UNSIGNED_BYTE, GL_TRUE},
        // U_RGB8
        {3, GL_UNSIGNED_BYTE, GL_TRUE},
        // U_RGBA8
        {4, GL_UNSIGNED_BYTE, GL_TRUE},
        // U_R16
        {1, GL_UNSIGNED_SHORT, GL_TRUE},
        // U_RG16
        {2, GL_UNSIGNED_SHORT, GL_TRUE},
        // U_RGB16
        {3, GL_UNSIGNED_SHORT, GL_TRUE},
        // U_RGBA16
        {4, GL_UNSIGNED_SHORT, GL_TRUE},
        // U_R32
        {1, GL_UNSIGNED_INT, GL_FALSE},
        // U_RG32
        {2, GL_UNSIGNED_INT, GL_FALSE},
        // U_RGB32
        {3, GL_UNSIGNED_INT, GL_FALSE},
        // U_RGBA32
        {4, GL_UNSIGNED_INT, GL_FALSE},
    };

    const GLVertexFormat &FromFormat(Format format)
    {
        return VERTEX_FORMAT_TABLE[static_cast<uint32_t>(format)];
    }

    // -----------------------------------------------------------------------
    // Sampler
    // -----------------------------------------------------------------------
    GLenum FromFilter(Filter filter)
    {
        static const GLenum TABLE[] = {
            GL_NEAREST, // NEAREST
            GL_LINEAR,  // LINEAR
        };
        return TABLE[static_cast<uint32_t>(filter)];
    }

    GLenum FromFilterMip(Filter filter, MipFilter mip)
    {
        if (mip == MipFilter::LINEAR) {
            return (filter == Filter::LINEAR) ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR;
        }
        return (filter == Filter::LINEAR) ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST;
    }

    GLenum FromWrapMode(WrapMode mode)
    {
        static const GLenum TABLE[] = {
            GL_REPEAT,                  // REPEAT
            GL_MIRRORED_REPEAT,         // MIRRORED_REPEAT
            GL_CLAMP_TO_EDGE,           // CLAMP_TO_EDGE
            GL_CLAMP_TO_BORDER,         // CLAMP_TO_BORDER  (GLES 3.2)
            GL_MIRROR_CLAMP_TO_EDGE_EXT // MIRROR_CLAMP_TO_EDGE
        };
        return TABLE[static_cast<uint32_t>(mode)];
    }

    // -----------------------------------------------------------------------
    // Pipeline state
    // -----------------------------------------------------------------------
    GLenum FromBlendFactor(BlendFactor factor)
    {
        static const GLenum TABLE[] = {
            GL_ZERO,                     // ZERO
            GL_ONE,                      // ONE
            GL_SRC_COLOR,                // SRC_COLOR
            GL_ONE_MINUS_SRC_COLOR,      // ONE_MINUS_SRC_COLOR
            GL_DST_COLOR,                // DST_COLOR
            GL_ONE_MINUS_DST_COLOR,      // ONE_MINUS_DST_COLOR
            GL_SRC_ALPHA,                // SRC_ALPHA
            GL_ONE_MINUS_SRC_ALPHA,      // ONE_MINUS_SRC_ALPHA
            GL_DST_ALPHA,                // DST_ALPHA
            GL_ONE_MINUS_DST_ALPHA,      // ONE_MINUS_DST_ALPHA
            GL_CONSTANT_COLOR,           // CONSTANT_COLOR
            GL_ONE_MINUS_CONSTANT_COLOR, // ONE_MINUS_CONSTANT_COLOR
            GL_CONSTANT_ALPHA,           // CONSTANT_ALPHA
            GL_ONE_MINUS_CONSTANT_ALPHA, // ONE_MINUS_CONSTANT_ALPHA
            GL_SRC_ALPHA_SATURATE,       // SRC_ALPHA_SATURATE
            GL_SRC1_COLOR_EXT,           // SRC1_COLOR
            GL_ONE_MINUS_SRC1_COLOR_EXT, // ONE_MINUS_SRC1_COLOR
            GL_SRC1_ALPHA_EXT,           // SRC1_ALPHA
            GL_ONE_MINUS_SRC1_ALPHA_EXT, // ONE_MINUS_SRC1_ALPHA
        };
        return TABLE[static_cast<uint32_t>(factor)];
    }

    GLenum FromBlendOp(BlendOp op)
    {
        static const GLenum TABLE[] = {
            GL_FUNC_ADD,             // ADD
            GL_FUNC_SUBTRACT,        // SUBTRACT
        };
        return TABLE[static_cast<uint32_t>(op)];
    }

    GLenum FromCompareOp(CompareOp op)
    {
        static const GLenum TABLE[] = {
            GL_NEVER,    // NEVER
            GL_LESS,     // LESS
            GL_EQUAL,    // EQUAL
            GL_LEQUAL,   // LESS_OR_EQUAL
            GL_GREATER,  // GREATER
            GL_NOTEQUAL, // NOT_EQUAL
            GL_GEQUAL,   // GREATER_OR_EQUAL
            GL_ALWAYS,   // ALWAYS
        };
        return TABLE[static_cast<uint32_t>(op)];
    }

    GLenum FromStencilOp(StencilOp op)
    {
        static const GLenum TABLE[] = {
            GL_KEEP,      // KEEP
            GL_ZERO,      // ZERO
            GL_REPLACE,   // REPLACE
            GL_INCR,      // INCREMENT_AND_CLAMP
            GL_DECR,      // DECREMENT_AND_CLAMP
            GL_INVERT,    // INVERT
            GL_INCR_WRAP, // INCREMENT_AND_WRAP
            GL_DECR_WRAP, // DECREMENT_AND_WRAP
        };
        return TABLE[static_cast<uint32_t>(op)];
    }

    GLenum FromPrimitiveTopology(PrimitiveTopology topo)
    {
        static const GLenum TABLE[] = {
            GL_POINTS,         // POINT_LIST
            GL_LINES,          // LINE_LIST
            GL_LINE_STRIP,     // LINE_STRIP
            GL_TRIANGLES,      // TRIANGLE_LIST
            GL_TRIANGLE_STRIP, // TRIANGLE_STRIP
            GL_TRIANGLE_FAN,   // TRIANGLE_FAN
        };
        return TABLE[static_cast<uint32_t>(topo)];
    }

    GLenum FromFrontFace(FrontFace face)
    {
        static const GLenum TABLE[] = {
            GL_CW,  // CW
            GL_CCW, // CCW
        };
        return TABLE[static_cast<uint32_t>(face)];
    }

    GLenum FromCullMode(const CullingModeFlags &flags)
    {
        const auto bits = static_cast<uint32_t>(flags);
        const auto front = static_cast<uint32_t>(CullModeFlagBits::FRONT);
        const auto back  = static_cast<uint32_t>(CullModeFlagBits::BACK);
        if ((bits & front) && (bits & back)) {
            return GL_FRONT_AND_BACK;
        }
        if (bits & front) {
            return GL_FRONT;
        }
        if (bits & back) {
            return GL_BACK;
        }
        return GL_NONE;
    }

    GLenum FromIndexType(IndexType type)
    {
        static const GLenum TABLE[] = {
            0,                    // NONE
            GL_UNSIGNED_SHORT,    // U16
            GL_UNSIGNED_INT,      // U32
        };
        return TABLE[static_cast<uint32_t>(type)];
    }

} // namespace sky::aurora
