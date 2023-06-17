//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/Conversion.h>
#include <unordered_map>

namespace sky::gles {

    const std::unordered_map<rhi::PixelFormat, InternalFormat> FORMAT_MAP = {
        {rhi::PixelFormat::UNDEFINED,                 {0,               0,       0}},
        {rhi::PixelFormat::RGB8_UNORM,                {GL_RGB8,         GL_RGB,  GL_UNSIGNED_BYTE}},
        {rhi::PixelFormat::RGB8_SRGB,                 {GL_SRGB8,        GL_RGB,  GL_UNSIGNED_BYTE}},

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

    const std::unordered_map<rhi::PixelFormat, FormatFeature> FORMAT_FEATURE = {
        //                                             texture, render, compressed
        {rhi::PixelFormat::UNDEFINED,                 {false,   false,  false}},
        {rhi::PixelFormat::RGB8_UNORM,                {true,    true,   false}},
        {rhi::PixelFormat::RGB8_SRGB,                 {true,    true,   false}},
        {rhi::PixelFormat::RGBA8_UNORM,               {true,    true,   false}},
        {rhi::PixelFormat::RGBA8_SRGB,                {true,    true,   false}},
        {rhi::PixelFormat::BGRA8_UNORM,               {true,    true,   false}},
        {rhi::PixelFormat::BGRA8_SRGB,                {true,    true,   false}},
        {rhi::PixelFormat::D32,                       {true,    true,   false}},
        {rhi::PixelFormat::D24_S8,                    {true,    true,   false}},
        {rhi::PixelFormat::D32_S8,                    {true,    true,   false}},
        {rhi::PixelFormat::BC1_RGB_UNORM_BLOCK,       {true,    false,  true}},
        {rhi::PixelFormat::BC1_RGB_SRGB_BLOCK,        {true,    false,  true}},
        {rhi::PixelFormat::BC1_RGBA_UNORM_BLOCK,      {true,    false,  true}},
        {rhi::PixelFormat::BC1_RGBA_SRGB_BLOCK,       {true,    false,  true}},
        {rhi::PixelFormat::BC2_UNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC2_SRGB_BLOCK,            {true,    false,  true}},
        {rhi::PixelFormat::BC3_UNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC3_SRGB_BLOCK,            {true,    false,  true}},
        {rhi::PixelFormat::BC4_UNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC4_SNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC5_UNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC5_SNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC6H_UFLOAT_BLOCK,         {true,    false,  true}},
        {rhi::PixelFormat::BC6H_SFLOAT_BLOCK,         {true,    false,  true}},
        {rhi::PixelFormat::BC7_UNORM_BLOCK,           {true,    false,  true}},
        {rhi::PixelFormat::BC7_SRGB_BLOCK,            {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8_UNORM_BLOCK,   {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8_SRGB_BLOCK,    {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8A1_UNORM_BLOCK, {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8A1_SRGB_BLOCK,  {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8A8_UNORM_BLOCK, {true,    false,  true}},
        {rhi::PixelFormat::ETC2_R8G8B8A8_SRGB_BLOCK,  {true,    false,  true}},
        {rhi::PixelFormat::ASTC_4x4_UNORM_BLOCK,      {true,    false,  true}},
        {rhi::PixelFormat::ASTC_4x4_SRGB_BLOCK,       {true,    false,  true}},
        {rhi::PixelFormat::ASTC_8x8_UNORM_BLOCK,      {true,    false,  true}},
        {rhi::PixelFormat::ASTC_8x8_SRGB_BLOCK,       {true,    false,  true}},
        {rhi::PixelFormat::ASTC_10x10_UNORM_BLOCK,    {true,    false,  true}},
        {rhi::PixelFormat::ASTC_10x10_SRGB_BLOCK,     {true,    false,  true}},
        {rhi::PixelFormat::ASTC_12x12_UNORM_BLOCK,    {true,    false,  true}},
        {rhi::PixelFormat::ASTC_12x12_SRGB_BLOCK,     {true,    false,  true}},
    };

    const std::unordered_map<rhi::Format, VertexFormat> VERTEX_FORMAT_TABLE = {
        //                            size   type   normalized
        {rhi::Format::UNDEFINED, {0, 0,        0}},
        {rhi::Format::F_R32,     {1, GL_FLOAT, GL_FALSE}},
        {rhi::Format::F_RG32,    {2, GL_FLOAT, GL_FALSE}},
        {rhi::Format::F_RGB32,   {3, GL_FLOAT, GL_FALSE}},
        {rhi::Format::F_RGBA32,  {4, GL_FLOAT, GL_FALSE}},
    };

    const std::unordered_map<rhi::WrapMode, GLenum> WRAP_MODE_MAP = {
        {rhi::WrapMode::REPEAT,               GL_REPEAT},
        {rhi::WrapMode::MIRRORED_REPEAT,      GL_MIRRORED_REPEAT},
        {rhi::WrapMode::CLAMP_TO_EDGE,        GL_CLAMP_TO_EDGE},
        {rhi::WrapMode::CLAMP_TO_BORDER,      GL_CLAMP_TO_BORDER},
        {rhi::WrapMode::MIRROR_CLAMP_TO_EDGE, GL_MIRROR_CLAMP_TO_EDGE_EXT},
    };

    const std::unordered_map<rhi::CompareOp, GLenum> COMPARE_OP_MAP = {
        {rhi::CompareOp::NEVER           , GL_NEVER},
        {rhi::CompareOp::LESS            , GL_LESS},
        {rhi::CompareOp::EQUAL           , GL_EQUAL},
        {rhi::CompareOp::LESS_OR_EQUAL   , GL_LEQUAL},
        {rhi::CompareOp::GREATER         , GL_GREATER},
        {rhi::CompareOp::NOT_EQUAL       , GL_NOTEQUAL},
        {rhi::CompareOp::GREATER_OR_EQUAL, GL_GEQUAL},
        {rhi::CompareOp::ALWAYS          , GL_ALWAYS},
    };

    const std::unordered_map<rhi::StencilOp, GLenum> STENCIL_OP_MAP = {
        {rhi::StencilOp::KEEP                , GL_KEEP},
        {rhi::StencilOp::ZERO                , GL_ZERO},
        {rhi::StencilOp::REPLACE             , GL_REPLACE},
        {rhi::StencilOp::INCREMENT_AND_CLAMP , GL_INCR},
        {rhi::StencilOp::DECREMENT_AND_CLAMP , GL_DECR},
        {rhi::StencilOp::INVERT              , GL_INVERT},
        {rhi::StencilOp::INCREMENT_AND_WRAP  , GL_INCR_WRAP},
        {rhi::StencilOp::DECREMENT_AND_WRAP  , GL_DECR_WRAP},
    };

    const std::unordered_map<rhi::BlendFactor, GLenum> BLEND_FACTOR_MAP = {
        {rhi::BlendFactor::ZERO                     , GL_ZERO},
        {rhi::BlendFactor::ONE                      , GL_ONE},
        {rhi::BlendFactor::SRC_COLOR                , GL_SRC_COLOR},
        {rhi::BlendFactor::ONE_MINUS_SRC_COLOR      , GL_ONE_MINUS_SRC_COLOR},
        {rhi::BlendFactor::DST_COLOR                , GL_DST_COLOR},
        {rhi::BlendFactor::ONE_MINUS_DST_COLOR      , GL_ONE_MINUS_DST_COLOR},
        {rhi::BlendFactor::SRC_ALPHA                , GL_SRC_ALPHA},
        {rhi::BlendFactor::ONE_MINUS_SRC_ALPHA      , GL_ONE_MINUS_SRC_ALPHA},
        {rhi::BlendFactor::DST_ALPHA                , GL_DST_ALPHA},
        {rhi::BlendFactor::ONE_MINUS_DST_ALPHA      , GL_ONE_MINUS_DST_ALPHA},
        {rhi::BlendFactor::CONSTANT_COLOR           , GL_CONSTANT_COLOR},
        {rhi::BlendFactor::ONE_MINUS_CONSTANT_COLOR , GL_ONE_MINUS_CONSTANT_COLOR},
        {rhi::BlendFactor::CONSTANT_ALPHA           , GL_CONSTANT_ALPHA},
        {rhi::BlendFactor::ONE_MINUS_CONSTANT_ALPHA , GL_ONE_MINUS_CONSTANT_ALPHA},
        {rhi::BlendFactor::SRC_ALPHA_SATURATE       , GL_SRC_ALPHA_SATURATE},
        {rhi::BlendFactor::SRC1_COLOR               , GL_SRC1_COLOR_EXT},
        {rhi::BlendFactor::ONE_MINUS_SRC1_COLOR     , GL_ONE_MINUS_SRC1_COLOR_EXT},
        {rhi::BlendFactor::SRC1_ALPHA               , GL_SRC1_ALPHA_EXT},
        {rhi::BlendFactor::ONE_MINUS_SRC1_ALPHA     , GL_ONE_MINUS_SRC1_ALPHA_EXT},
    };

    const std::unordered_map<rhi::BlendOp, GLenum> BLEND_OP_MAP = {
        {rhi::BlendOp::ADD,      GL_FUNC_ADD},
        {rhi::BlendOp::SUBTRACT, GL_FUNC_SUBTRACT}
    };

    const std::unordered_map<rhi::PrimitiveTopology, GLenum> TOPO_MAP = {
        {rhi::PrimitiveTopology::POINT_LIST     , GL_POINTS},
        {rhi::PrimitiveTopology::LINE_LIST      , GL_LINES},
        {rhi::PrimitiveTopology::LINE_STRIP     , GL_LINE_STRIP},
        {rhi::PrimitiveTopology::TRIANGLE_LIST  , GL_TRIANGLES},
        {rhi::PrimitiveTopology::TRIANGLE_STRIP , GL_TRIANGLE_STRIP},
        {rhi::PrimitiveTopology::TRIANGLE_FAN   , GL_TRIANGLE_FAN},
    };

    const InternalFormat &GetInternalFormat(rhi::PixelFormat format)
    {
        return FORMAT_MAP.find(format)->second;
    }

    const FormatFeature &GetFormatFeature(rhi::PixelFormat format)
    {
        return FORMAT_FEATURE.find(format)->second;
    }

    const VertexFormat &GetVertexFormat(rhi::Format format)
    {
        return VERTEX_FORMAT_TABLE.find(format)->second;
    }

    GLenum FromRHI(rhi::Filter filter, rhi::MipFilter mipFilter)
    {
        if (mipFilter == rhi::MipFilter::LINEAR) {
            if (filter == rhi::Filter::LINEAR)  return GL_LINEAR_MIPMAP_LINEAR;
            return GL_NEAREST_MIPMAP_LINEAR;
        } else {
            if (filter == rhi::Filter::LINEAR) return GL_LINEAR_MIPMAP_NEAREST;
            return GL_NEAREST_MIPMAP_NEAREST;
        }
    }

    GLenum FromRHI(rhi::Filter filter)
    {
        return filter == rhi::Filter::LINEAR ? GL_LINEAR : GL_NEAREST;
    }

    GLenum FromRHI(rhi::WrapMode mode)
    {
        return WRAP_MODE_MAP.find(mode)->second;
    }

    GLenum FromRHI(rhi::CompareOp compare)
    {
        return COMPARE_OP_MAP.find(compare)->second;
    }

    GLenum FromRHI(rhi::CullingModeFlags flags)
    {
        if (flags == (rhi::CullModeFlagBits::FRONT | rhi::CullModeFlagBits::BACK)) return GL_FRONT_AND_BACK;
        return flags == rhi::CullModeFlagBits::FRONT ? GL_FRONT : GL_BACK;
    }

    GLenum FromRHI(rhi::FrontFace frontFace)
    {
        return frontFace == rhi::FrontFace::CW ? GL_CW : GL_CCW;
    }

    GLenum FromRHI(rhi::PrimitiveTopology topo)
    {
        return TOPO_MAP.find(topo)->second;
    }

    GLenum FromRHI(rhi::StencilOp op)
    {
        return STENCIL_OP_MAP.find(op)->second;
    }

    GLenum FromRHI(rhi::BlendFactor factor)
    {
        return BLEND_FACTOR_MAP.find(factor)->second;
    }

    GLenum FromRHI(rhi::BlendOp op)
    {
        return BLEND_OP_MAP.find(op)->second;
    }

    StencilState FromRHI(const rhi::StencilState &state)
    {
        StencilState stencil;
        stencil.func      = FromRHI(state.compareOp);
        stencil.reference = state.reference;
        stencil.readMask  = state.compareMask;
        stencil.writemask = state.writeMask;
        stencil.failOp    = FromRHI(state.failOp);
        stencil.zFailOp   = FromRHI(state.depthFailOp);
        stencil.zPassOp   = FromRHI(state.passOp);
        return stencil;
    }

    BlendTarget FromRHI(const rhi::BlendState &state)
    {
        BlendTarget blend;
        blend.writeMask      = state.writeMask;
        blend.blendOp        = FromRHI(state.colorBlendOp);
        blend.blendAlphaOp   = FromRHI(state.alphaBlendOp);
        blend.blendSrc       = FromRHI(state.srcColor);
        blend.blendDst       = FromRHI(state.dstColor);
        blend.blendSrcAlpha  = FromRHI(state.srcAlpha);
        blend.blendDstAlpha  = FromRHI(state.dstAlpha);
        return blend;
    }
}
