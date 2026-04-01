//
// Created on 2026/04/01.
//

#include <GLESPipelineState.h>
#include <GLESLoader.h>
#include <GLESShader.h>
#include <GLESConversion.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    bool GLESGraphicsPipeline::Init(const Descriptor &desc)
    {
        if (desc.shader != nullptr) {
            auto *glesShader = static_cast<GLESShader *>(desc.shader);
            program = glesShader->GetProgram();
        }

        if (program == 0) {
            LOG_E(TAG, "pipeline state requires a linked shader program");
            return false;
        }

        CacheState(desc);
        return true;
    }

    void GLESGraphicsPipeline::CacheState(const Descriptor &desc)
    {
        const auto *state = desc.state;
        if (state == nullptr) {
            return;
        }

        // input assembly
        topology = FromPrimitiveTopology(state->inputAssembly.topology);

        // raster
        frontFace = FromFrontFace(state->rasterState.frontFace);
        cullFace  = FromCullMode(state->rasterState.cullMode);
        // Note: GLES does not support glPolygonMode; wireframe is not available

        // depth stencil
        depthTestEnable  = state->depthStencil.depthTest;
        depthWriteEnable = state->depthStencil.depthWrite;
        depthFunc        = FromCompareOp(state->depthStencil.compareOp);

        stencilTestEnable = state->depthStencil.stencilTest;
        if (stencilTestEnable) {
            const auto &front = state->depthStencil.front;
            stencilFrontFunc      = FromCompareOp(front.compareOp);
            stencilFrontFail      = FromStencilOp(front.failOp);
            stencilFrontDFail     = FromStencilOp(front.depthFailOp);
            stencilFrontDPass     = FromStencilOp(front.passOp);
            stencilFrontRef       = front.reference;
            stencilFrontMask      = front.compareMask;
            stencilFrontWriteMask = front.writeMask;

            const auto &back = state->depthStencil.back;
            stencilBackFunc      = FromCompareOp(back.compareOp);
            stencilBackFail      = FromStencilOp(back.failOp);
            stencilBackDFail     = FromStencilOp(back.depthFailOp);
            stencilBackDPass     = FromStencilOp(back.passOp);
            stencilBackRef       = back.reference;
            stencilBackMask      = back.compareMask;
            stencilBackWriteMask = back.writeMask;
        }

        // blend (first target)
        if (!state->blendStates.empty()) {
            const auto &b = state->blendStates[0];
            blendEnable    = b.blendEn;
            srcColorFactor = FromBlendFactor(b.srcColor);
            dstColorFactor = FromBlendFactor(b.dstColor);
            colorOp        = FromBlendOp(b.colorBlendOp);
            srcAlphaFactor = FromBlendFactor(b.srcAlpha);
            dstAlphaFactor = FromBlendFactor(b.dstAlpha);
            alphaOp        = FromBlendOp(b.alphaBlendOp);
        }
    }

    void GLESGraphicsPipeline::Bind() const
    {
        glUseProgram(program);

        // raster
        if (cullFace != GL_NONE) {
            glEnable(GL_CULL_FACE);
            glCullFace(cullFace);
        } else {
            glDisable(GL_CULL_FACE);
        }
        glFrontFace(frontFace);

        // depth
        if (depthTestEnable) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(depthFunc);
            glDepthMask(depthWriteEnable ? GL_TRUE : GL_FALSE);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        // stencil
        if (stencilTestEnable) {
            glEnable(GL_STENCIL_TEST);
            glStencilFuncSeparate(GL_FRONT, stencilFrontFunc, stencilFrontRef, stencilFrontMask);
            glStencilOpSeparate(GL_FRONT, stencilFrontFail, stencilFrontDFail, stencilFrontDPass);
            glStencilMaskSeparate(GL_FRONT, stencilFrontWriteMask);

            glStencilFuncSeparate(GL_BACK, stencilBackFunc, stencilBackRef, stencilBackMask);
            glStencilOpSeparate(GL_BACK, stencilBackFail, stencilBackDFail, stencilBackDPass);
            glStencilMaskSeparate(GL_BACK, stencilBackWriteMask);
        } else {
            glDisable(GL_STENCIL_TEST);
        }

        // blend
        if (blendEnable) {
            glEnable(GL_BLEND);
            glBlendFuncSeparate(srcColorFactor, dstColorFactor, srcAlphaFactor, dstAlphaFactor);
            glBlendEquationSeparate(colorOp, alphaOp);
        } else {
            glDisable(GL_BLEND);
        }
    }

} // namespace sky::aurora
