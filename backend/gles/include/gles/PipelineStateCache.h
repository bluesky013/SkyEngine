//
// Created by Zach Lee on 2023/4/1.
//

#pragma once

#include <gles/Forward.h>
#include <gles/Core.h>
#include <core/math/Color.h>

namespace sky::gles {
    struct PipelineCacheState {
        RasterizerState   rs;
        DepthStencilState ds;
        GLColor           color;
        BlendTarget       target;
        bool              isA2C      = false;
        GLenum            primitive   = GL_TRIANGLES;
        GLenum            indexType   = GL_UNSIGNED_INT;
        GLuint            program     = 0;
        GLuint            drawBuffer  = 0;
        GLuint            vao         = 0;
        bool              scissorTest = false;
        rhi::Viewport     viewport    = {0, 0, 1, 1, 0.f, 1.f};
        rhi::Rect2D       scissor     = {0, 0, 1, 1};
    };
}