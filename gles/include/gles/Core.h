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

    struct GLStencil {
        GLenum compFunc  = GL_ALWAYS;
        GLint reference  = 0;
        GLuint readMask  = 0xFFFFFFFF;
        GLuint writeMask = 0xFFFFFFFF;
        GLenum failOP    = GL_KEEP;
        GLenum dpFailOp  = GL_KEEP;
        GLenum dpPassOp  = GL_KEEP;
    };

    struct GLBlend {
        GLuint writeMask      = 0xF;
        GLenum blendFuncColor = GL_FUNC_ADD;
        GLenum blendFuncAlpha = GL_FUNC_ADD;
        GLenum blendSrcColor  = GL_ONE;
        GLenum blendDstColor  = GL_ZERO;
        GLenum blendSrcAlpha  = GL_ONE;
        GLenum blendDstAlpha  = GL_ZERO;
    };

    struct GLState {
        bool cullingFaceEn      = false; //        GL_CULL_FACE
        bool depthTestEn        = false; //        GL_DEPTH_TEST
        bool ditherEn           = true;  //        GL_DITHER
        bool polygonOffsetEn    = false; //        GL_POLYGON_OFFSET_FILL
        bool primitiveRestartEn = false; //        GL_PRIMITIVE_RESTART_FIXED_INDEX
        bool rasterizerDiscard  = false; //        GL_RASTERIZER_DISCARD
        bool alphaToCoverage    = false; //        GL_SAMPLE_ALPHA_TO_COVERAGE
        bool samplerCoverage    = false; //        GL_SAMPLE_COVERAGE
        bool scissorTest        = false; //        GL_SCISSOR_TEST
        bool stencilTest        = false; //        GL_STENCIL_TEST

        bool depthMask   = true;
        GLenum depthFunc = GL_LESS;
        GLfloat minDepth = 0.f;
        GLfloat maxDepth = 1.f;

        GLStencil front;
        GLStencil back;

        GLenum cullingMode = GL_BACK;
        GLenum frontFace   = GL_CCW;
        GLfloat polygonConstant = 0.f;
        GLfloat polygonUnits    = 0.f;
        GLfloat lineWidth       = 1.f;

        GLenum primitive = GL_TRIANGLES;

        GLColor blendColor;
        std::vector<GLBlend> blendStates;
    };

    struct GLDescriptorIndex {
        union {
            uint32_t binding;
            uint32_t unit;
        };
    };
}
