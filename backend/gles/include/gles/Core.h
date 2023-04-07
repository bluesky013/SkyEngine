//
// Created by Zach on 2023/2/1.
//

#pragma once

#include <core/platform/Platform.h>
#include <gles/Forward.h>

namespace sky::gles {

#ifdef _DEBUG
#define CHECK(x) do { x; SKY_ASSERT(glGetError() == GL_NO_ERROR); } while(0);
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
}
