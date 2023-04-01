//
// Created by Zach Lee on 2023/4/1.
//

#pragma once

#include <gles/Forward.h>
#include <core/math/Color.h>

namespace sky::gles {
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
        bool        hasColor = false;
        Color       color;
        BlendTarget target;
    };

    struct DepthStencilState {
        DepthState   depth;
        bool         stencilTest = false;
        StencilState front;
        StencilState back;
    };

    struct PipelineCacheState {
        RasterizerState   rs;
        DepthStencilState ds;
        BlendState        bs;
        GLenum            primitive   = GL_TRIANGLES;
        GLuint            program     = 0;
        GLuint            drawBuffer  = 0;
        GLuint            vao         = 0;
        bool              scissorTest = false;
        rhi::Viewport     viewport    = {0, 0, 1, 1, 0.f, 1.f};
        rhi::Rect2D       scissor     = {0, 0, 1, 1};
    };
}