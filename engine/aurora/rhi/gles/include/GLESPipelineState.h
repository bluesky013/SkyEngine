//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/PipelineState.h>
#include <GLESForward.h>

namespace sky::aurora {

    // In GLES there is no monolithic PSO object. This class caches
    // the GL program handle plus the rasterizer / depth-stencil / blend
    // state descriptors that are applied at draw time via glEnable/glDisable.
    class GLESGraphicsPipeline : public GraphicsPipeline {
    public:
        GLESGraphicsPipeline() = default;
        ~GLESGraphicsPipeline() override = default;

        bool Init(const Descriptor &desc);

        GLuint GetProgram() const { return program; }

        // Bind the cached state and program for drawing.
        void Bind() const;

    private:
        void CacheState(const Descriptor &desc);

        GLuint program = 0; // borrowed from GLESShader, not owned

        // cached raster state
        GLenum  topology  = GL_TRIANGLES;
        GLenum  frontFace = GL_CCW;
        GLenum  cullFace  = GL_NONE;
        // Note: GLES does not support glPolygonMode (no GL_FILL/GL_LINE)

        // depth-stencil
        bool    depthTestEnable   = false;
        bool    depthWriteEnable  = true;
        GLenum  depthFunc         = GL_LESS;
        bool    stencilTestEnable = false;
        GLenum  stencilFrontFunc = GL_ALWAYS;
        GLenum  stencilFrontFail = GL_KEEP;
        GLenum  stencilFrontDFail = GL_KEEP;
        GLenum  stencilFrontDPass = GL_KEEP;
        uint32_t stencilFrontRef  = 0;
        uint32_t stencilFrontMask = 0xFF;
        uint32_t stencilFrontWriteMask = 0xFF;
        GLenum  stencilBackFunc  = GL_ALWAYS;
        GLenum  stencilBackFail  = GL_KEEP;
        GLenum  stencilBackDFail = GL_KEEP;
        GLenum  stencilBackDPass = GL_KEEP;
        uint32_t stencilBackRef   = 0;
        uint32_t stencilBackMask  = 0xFF;
        uint32_t stencilBackWriteMask = 0xFF;

        // blend (first target only for simplicity; expand if needed)
        bool    blendEnable    = false;
        GLenum  srcColorFactor = GL_ONE;
        GLenum  dstColorFactor = GL_ZERO;
        GLenum  colorOp        = GL_FUNC_ADD;
        GLenum  srcAlphaFactor = GL_ONE;
        GLenum  dstAlphaFactor = GL_ZERO;
        GLenum  alphaOp        = GL_FUNC_ADD;
    };

} // namespace sky::aurora
