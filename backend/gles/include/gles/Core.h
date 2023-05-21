//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <core/platform/Platform.h>
#include <core/logger/Logger.h>
#include <gles/Forward.h>

namespace sky::gles {

#ifdef _DEBUG
#define CHECK(x) do { x; auto error = glGetError(); if (error != GL_NO_ERROR) { LOG_I("GLES", "gl error %u", error); SKY_ASSERT(false); } } while(0);
#else
#define CHECK(x) do { x; } while(0);
#endif
    struct GLColor {
        GLfloat red   = 0.f;
        GLfloat green = 0.f;
        GLfloat blue  = 0.f;
        GLfloat alpha = 0.f;
    };

    struct RasterizerState {
        bool   cullingEn = false;
        GLenum cullFace  = GL_BACK;
        GLenum frontFace = GL_CCW;

        GLfloat depthBias     = 0.F;
        GLfloat depthBiasSlop = 0.F;
        GLfloat lineWidth     = 1.F;
    };

    struct StencilState {
        GLenum   func      = GL_ALWAYS;
        uint32_t readMask  = 0xffffffff;
        uint32_t writemask = 0xffffffff;
        uint32_t reference = 0;
        GLenum   failOp    = GL_KEEP;
        GLenum   zPassOp   = GL_KEEP;
        GLenum   zFailOp   = GL_KEEP;
    };

    struct DepthState {
        bool    depthWrite = true;
        bool    depthTest  = false;
        GLenum  depthFunc  = GL_LESS;
        GLfloat minDepth   = 0.F;
        GLfloat maxDepth   = 1.F;
    };

    struct BlendTarget {
        bool    blendEnable   = false;
        uint8_t writeMask     = 0xF;
        GLenum  blendOp       = GL_FUNC_ADD;
        GLenum  blendSrc      = GL_ONE;
        GLenum  blendDst      = GL_ZERO;
        GLenum  blendAlphaOp  = GL_FUNC_ADD;
        GLenum  blendSrcAlpha = GL_ONE;
        GLenum  blendDstAlpha = GL_ZERO;
    };

    struct BlendState {
        bool        isA2C    = false;
        GLColor     color;
        std::vector<BlendTarget> target;
    };

    struct DepthStencilState {
        DepthState   depth;
        bool         stencilTest = false;
        StencilState front;
        StencilState back;
    };

    struct GLState {
        RasterizerState   rs;
        DepthStencilState ds;
        BlendState        bs;
        GLenum primitive = GL_TRIANGLES;
    };

    struct GLDescriptorIndex {
        union {
            uint32_t binding;
            uint32_t unit;
        };
    };

    inline bool HasDepth(rhi::PixelFormat format)
    {
        return format == rhi::PixelFormat::D24_S8 ||
               format == rhi::PixelFormat::D32 ||
               format == rhi::PixelFormat::D32_S8;
    }

    inline bool HasStencil(rhi::PixelFormat format)
    {
        return format == rhi::PixelFormat::D32_S8 ||
               format == rhi::PixelFormat::D24_S8;
    }
}
