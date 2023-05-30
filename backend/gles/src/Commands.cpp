//
// Created by Zach Lee on 2023/2/5.
//

#include <gles/CommandContext.h>
#include <core/logger/Logger.h>

#include <gles/Ext.h>
#include <gles/Core.h>
#include <gles/Queue.h>
#include <gles/GraphicsPipeline.h>
#include <gles/Device.h>

namespace sky::gles {

    static void SetEnable(bool status, GLenum flag) {
        if (status) {
            CHECK(glEnable(flag));
        } else {
            CHECK(glDisable(flag));
        }
    }

    static bool IsDynamicBuffer(rhi::DescriptorType type)
    {
        return type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC || type == rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC;
    }

    static bool IsBufferType(rhi::DescriptorType type)
    {
        return type == rhi::DescriptorType::STORAGE_BUFFER ||
            type == rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC ||
            type == rhi::DescriptorType::UNIFORM_BUFFER ||
            type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC;
    }

    static bool IsCombinedSampler(rhi::DescriptorType type)
    {
        return type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER;
    }

    static bool CheckBuffer(const SetDescriptor &desc) {
        return desc.buffer.view && desc.buffer.view && desc.buffer.view->GetNativeHandle() != 0 && desc.buffer.view->GetViewDesc().range != 0;
    }

    static bool CheckTexture(const SetDescriptor& desc) {
        return desc.texture.view && desc.texture.view->GetImage() && desc.texture.view->GetImage()->GetNativeHandle() != 0;
    }

    static GLenum GetBufferTarget(rhi::DescriptorType type) {
        switch (type) {
            case rhi::DescriptorType::STORAGE_BUFFER:
            case rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC:
                return GL_SHADER_STORAGE_BUFFER;
            case rhi::DescriptorType::UNIFORM_BUFFER:
            case rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC:
                return GL_UNIFORM_BUFFER;
            default:
                return 0;
        }
    }

    static GLenum GetTextureTarget(const ImageViewPtr &imageView)
    {
        const auto &viewInfo = imageView->GetViewDesc();
        switch (viewInfo.viewType) {
        case rhi::ImageViewType::VIEW_2D: return GL_TEXTURE_2D;
        case rhi::ImageViewType::VIEW_2D_ARRAY: return GL_TEXTURE_2D_ARRAY;
        case rhi::ImageViewType::VIEW_CUBE: return GL_TEXTURE_CUBE_MAP;
        case rhi::ImageViewType::VIEW_CUBE_ARRAY: return GL_TEXTURE_CUBE_MAP_ARRAY;
        case rhi::ImageViewType::VIEW_3D: return GL_TEXTURE_3D;
        }
        return 0;
    }

    void CommandContext::CmdBeginPass(const FrameBufferPtr &frameBuffer, const RenderPassPtr &renderPass, uint32_t count, rhi::ClearValue *values)
    {
        currentFramebuffer = frameBuffer;
        currentRenderPass  = renderPass;
        clearValues = values;
        clearCount = count;

        const auto &ext = currentFramebuffer->GetExtent();
        CHECK(glViewport(0, 0, ext.width, ext.height));
        CHECK(glScissor(0, 0, ext.width, ext.height));

        BeginPassInternal();
    }

    void CommandContext::CmdNextPass()
    {
        ++currentSubPassId;
//        if (FramebufferFetchBarrier != nullptr) {
//            CHECK(FramebufferFetchBarrier());
//        }
    }

    void CommandContext::BeginPassInternal()
    {
        const auto &subPasses = currentRenderPass->GetSubPasses();
        const auto &attachments = currentRenderPass->GetAttachments();
        const auto &attachmentInfos = currentRenderPass->GetAttachmentGLInfos();
        const auto &attachmentMap = currentRenderPass->GetAttachmentColorMap();

        SKY_ASSERT(currentSubPassId < subPasses.size());

        const auto &surface = currentFramebuffer->GetSurface();
        GLuint fbo = 0;
        if (surface && surface->GetSurface() != context->GetCurrentSurface()) {
            context->MakeCurrent(*surface);
        }
        if (!surface) {
            fbo = currentFramebuffer->AcquireNativeHandle(static_cast<uint32_t>(type));
        }
        if (cache->drawBuffer != fbo) {
            CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
            cache->drawBuffer = fbo;
        }

        GLbitfield clearBit = 0;
        auto attachmentFunc = [this, &clearBit](const rhi::RenderPass::Attachment &attachment, const RenderPass::AttachmentGLInfo &info, uint32_t colorIndex, uint32_t index) {
            bool isColor = !(info.hasDepth || info.hasStencil);
            bool isDefaultFb = cache->drawBuffer == 0;

            if (attachment.load == rhi::LoadOp::DONT_CARE) {
                if (info.hasDepth) {
                    invalidAttachments.emplace_back(isDefaultFb ? GL_DEPTH : GL_DEPTH_ATTACHMENT);
                }
                if (isColor) {
                    invalidAttachments.emplace_back(isDefaultFb ? GL_COLOR : GL_COLOR_ATTACHMENT0 + colorIndex);
                }
            } else if (attachment.load == rhi::LoadOp::CLEAR) {
                auto &clearColor = clearValues[index];
                if (cache->target.writeMask != 0xF) {
                    CHECK(glColorMask(true, true, true, true));
                    cache->target.writeMask = 0xF;
                }

                if (isColor) {
                    CHECK(glClearBufferfv(GL_COLOR, colorIndex, clearColor.color.float32));
                }
                if (info.hasDepth) {
                    if (!cache->ds.depth.depthWrite) {
                        CHECK(glDepthMask(true));
                        cache->ds.depth.depthWrite = true;
                    }
                    CHECK(glClearDepthf(clearColor.depthStencil.depth));
                    clearBit |= GL_DEPTH_BUFFER_BIT;
                }
            }

            if (info.hasStencil) {
                if (attachment.stencilLoad == rhi::LoadOp::DONT_CARE) {
                    invalidAttachments.emplace_back(isDefaultFb ? GL_STENCIL : GL_STENCIL_ATTACHMENT);
                } else if (attachment.stencilLoad == rhi::LoadOp::CLEAR) {
                    auto &clearColor = clearValues[index];
                    if (cache->ds.front.writemask != 0xffffffff) {
                        CHECK(glStencilMaskSeparate(GL_FRONT, 0xffffffff));
                        cache->ds.front.writemask = 0xffffffff;
                    }
                    if (cache->ds.back.writemask != 0xffffffff) {
                        CHECK(glStencilMaskSeparate(GL_BACK, 0xffffffff));
                        cache->ds.back.writemask = 0xffffffff;
                    }
                    CHECK(glClearStencil(clearColor.depthStencil.stencil));
                    clearBit |= GL_STENCIL_BUFFER_BIT;
                }
            }
        };

        for (uint32_t i = 0; i < attachments.size(); ++i) {
            attachmentFunc(attachments[i], attachmentInfos[i], attachmentMap[i], i);
        }

        if (clearBit != 0) {
            CHECK(glClear(clearBit));
        }

        if (!invalidAttachments.empty()) {
            CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, static_cast<uint32_t>(invalidAttachments.size()), invalidAttachments.data()));
            invalidAttachments.clear();
        }
    }

    void CommandContext::EndPassInternal()
    {
        const auto &subPasses = currentRenderPass->GetSubPasses();
        const auto &attachments = currentRenderPass->GetAttachments();
        const auto &attachmentInfos = currentRenderPass->GetAttachmentGLInfos();
        const auto &attachmentMap = currentRenderPass->GetAttachmentColorMap();

        SKY_ASSERT(currentSubPassId < subPasses.size());

        auto attachmentFunc = [this](const rhi::RenderPass::Attachment &attachment, const RenderPass::AttachmentGLInfo &info, uint32_t colorIndex) {
            bool isColor = !(info.hasDepth || info.hasStencil);
            bool isDefaultFb = cache->drawBuffer == 0;

            if (attachment.store == rhi::StoreOp::DONT_CARE) {
                if (info.hasDepth) {
                    invalidAttachments.emplace_back(isDefaultFb ? GL_DEPTH : GL_DEPTH_ATTACHMENT);
                }
                if (isColor) {
                    invalidAttachments.emplace_back(isDefaultFb ? GL_COLOR : GL_COLOR_ATTACHMENT0 + colorIndex);
                }
            }

            if (info.hasStencil && attachment.stencilStore == rhi::StoreOp::DONT_CARE) {
                invalidAttachments.emplace_back(isDefaultFb ? GL_STENCIL : GL_STENCIL_ATTACHMENT);
            }
        };

        // perform attachment store op
        for (uint32_t i = 0; i < attachments.size(); ++i) {
            attachmentFunc(attachments[i], attachmentInfos[i], attachmentMap[i]);
        }

        if (!invalidAttachments.empty()) {
            CHECK(glInvalidateFramebuffer(GL_FRAMEBUFFER, static_cast<uint32_t>(invalidAttachments.size()), invalidAttachments.data()));
            invalidAttachments.clear();
        }
    }

    void CommandContext::CmdBindDescriptorSet(uint32_t setId, const DescriptorSetPtr &set, uint32_t dynamicCount, uint32_t *dynamicOffsets)
    {
        SKY_ASSERT(setId < MAX_SET_ID);
        if (sets.empty()) {
            sets.resize(MAX_SET_ID);
        }
        sets[setId] = set;

        const auto &descriptorOffsets = currentPso->GetDescriptorOffsets();
        const auto &descriptorIndices = currentPso->GetDescriptorIndices();

        auto pipelineLayout = currentPso->GetPipelineLayout();
        const auto &setLayouts = pipelineLayout->GetLayouts();

        const auto &descriptorSetLayout = setLayouts[setId];
        const auto &bindings = descriptorSetLayout->GetBindings();
        const auto &bindingMap = descriptorSetLayout->GetBindingMap();

        uint32_t dynamicIndex = 0;
        for (const auto &binding : bindings) {
            auto offsetToSet            = bindingMap.at(binding.binding);
            auto offsetToPipelineLayout = descriptorOffsets[setId];

            uint32_t *dynamicOffsetPtr = nullptr;
            if (IsDynamicBuffer(binding.type) && dynamicIndex < dynamicCount) {
                dynamicOffsetPtr = &dynamicOffsets[dynamicIndex];
                dynamicIndex += binding.count;
            }

            const auto *descriptorIndexBase = &descriptorIndices[offsetToSet + offsetToPipelineLayout];
            const auto *descriptorBase      = &set->GetDescriptors()[offsetToSet];

            for (uint32_t i = 0; i < binding.count; ++i) {
                const auto &descriptor      = descriptorBase[i];
                const auto &descriptorIndex = descriptorIndexBase[i];
                if (IsBufferType(binding.type) && CheckBuffer(descriptor)) {
                    uint32_t dynamicOffset = dynamicOffsetPtr != nullptr ? dynamicOffsetPtr[i] : 0;
                    const auto &bufferView = descriptor.buffer.view;
                    const auto &viewInfo = bufferView->GetViewDesc();
                    CHECK(glBindBufferRange(GetBufferTarget(binding.type), descriptorIndex.binding, bufferView->GetNativeHandle(),
                        viewInfo.offset + dynamicOffset, viewInfo.range));
                } else if (IsCombinedSampler(binding.type) && CheckTexture(descriptor)) {
                    CHECK(glActiveTexture(GL_TEXTURE0 + descriptorIndex.unit));
                    CHECK(glBindTexture(GetTextureTarget(descriptor.texture.view), descriptor.texture.view->GetNativeHandle()));
                    CHECK(glBindSampler(descriptorIndex.unit, descriptor.texture.sampler->GetNativeHandle()));
                }
            }
        }
    }

    void CommandContext::SetStencilWriteMask(StencilFace face, uint32_t mask) {
        auto stencilFn = [](StencilState &state, GLenum face, uint32_t mask) {
            if (state.writemask != mask) {
                CHECK(glStencilMaskSeparate(face, mask));
                state.writemask = mask;
            }
        };

        if (face == StencilFace::FRONT) {
            stencilFn(cache->ds.front, GL_FRONT, mask);
        } else {
            stencilFn(cache->ds.back, GL_BACK, mask);
        }
    }

    void CommandContext::SetStencilFunc(StencilFace face, GLenum func, uint32_t ref, uint32_t mask) {
        auto stencilFn = [](StencilState &state, GLenum face, GLenum func, uint32_t ref, uint32_t mask) {
            if (state.readMask != mask || state.reference != ref || state.func != func) {
                CHECK(glStencilFuncSeparate(face, func, ref, mask));
                state.readMask = mask;
                state.reference = ref;
                state.func = func;
            }
        };
        if (face == StencilFace::FRONT) {
            stencilFn(cache->ds.front, GL_FRONT, func, ref, mask);
        } else {
            stencilFn(cache->ds.back, GL_BACK, func, ref, mask);
        }
    }

    void CommandContext::SetStencilOp(StencilFace face, GLenum sFail, GLenum dpFail, GLenum dpPass) {
        auto stencilFn = [](StencilState &state, GLenum face, GLenum sFail, GLenum dpFail, GLenum dpPass) {
            if (state.failOp != sFail || state.zPassOp != dpPass || state.zFailOp != dpFail) {
                CHECK(glStencilOpSeparate(face, sFail, dpFail, dpPass));
                state.failOp = sFail;
                state.zPassOp = dpPass;
                state.zFailOp = dpFail;
            }
        };
        if (face == StencilFace::FRONT) {
            stencilFn(cache->ds.front, GL_FRONT, sFail, dpFail, dpPass);
        } else {
            stencilFn(cache->ds.back, GL_BACK, sFail, dpFail, dpPass);
        }
    }

    void CommandContext::SetDepthBound(float minBounds, float maxBounds) {
        if (cache->ds.depth.minDepth != minBounds || cache->ds.depth.maxDepth != maxBounds) {
            CHECK(glDepthRangef(minBounds, maxBounds));
            cache->ds.depth.minDepth = minBounds;
            cache->ds.depth.maxDepth = maxBounds;
        }
    }

    void CommandContext::SetLineWidth(float width) {
        if (cache->rs.lineWidth != width) {
            CHECK(glLineWidth(width));
            cache->rs.lineWidth = width;
        }
    }

    void CommandContext::SetDepthBias(float constant, float clamp, float slope) {
        std::ignore = clamp;
        if (cache->rs.depthBias != constant || cache->rs.depthBiasSlop != slope) {
            CHECK(glPolygonOffset(constant, slope));
            cache->rs.depthBias = constant;
            cache->rs.depthBiasSlop = slope;
        }
    }

    void CommandContext::SetBlendState(const BlendState &bs) {
        auto &isA2C = cache->isA2C;
        if (isA2C != bs.isA2C) {
            SetEnable(bs.isA2C, GL_SAMPLE_ALPHA_TO_COVERAGE);
            isA2C = bs.isA2C;
        }

        auto &color = cache->color;
        if (color.red != bs.color.red || color.blue != bs.color.blue || color.green != bs.color.green || color.alpha != bs.color.green) {
            CHECK(glBlendColor(bs.color.red, bs.color.green, bs.color.blue, bs.color.alpha));
            color = bs.color;
        }

        if (!bs.target.empty()) {
            const auto &target = bs.target[0];
            auto &currentTarget = cache->target;
            if (target.blendEnable != currentTarget.blendEnable) {
                SetEnable(target.blendEnable, GL_BLEND);
                currentTarget.blendEnable = target.blendEnable;
            }
            if (target.writeMask != currentTarget.writeMask) {
                CHECK(glColorMask(target.writeMask & 0x01, target.writeMask & 0x02, target.writeMask & 0x04, target.writeMask & 0x08));
                currentTarget.writeMask = target.writeMask;
            }
            if (currentTarget.blendOp != target.blendOp ||
                currentTarget.blendAlphaOp != target.blendAlphaOp) {
                CHECK(glBlendEquationSeparate(target.blendOp, target.blendAlphaOp));
                currentTarget.blendOp = target.blendOp;
                currentTarget.blendAlphaOp = target.blendAlphaOp;
            }
            if (currentTarget.blendSrc != target.blendSrc ||
                currentTarget.blendDst != target.blendDst ||
                currentTarget.blendSrcAlpha != target.blendSrcAlpha ||
                currentTarget.blendDstAlpha != target.blendDstAlpha) {
                CHECK(glBlendFuncSeparate(target.blendSrc, target.blendDst, target.blendSrcAlpha, target.blendDstAlpha));
                currentTarget.blendSrc      = target.blendSrc;
                currentTarget.blendDst      = target.blendDst;
                currentTarget.blendSrcAlpha = target.blendSrcAlpha;
                currentTarget.blendDstAlpha = target.blendDstAlpha;
            }
        }
    }

    void CommandContext::SetRasterizerState(const RasterizerState &rs) {
        auto &currentRs = cache->rs;
        if (currentRs.cullingEn != rs.cullingEn) {
            SetEnable(rs.cullingEn, GL_CULL_FACE);
            currentRs.cullingEn = rs.cullingEn;
        }

        if (rs.cullingEn) {
            if (currentRs.cullFace != rs.cullFace) {
                CHECK(glCullFace(rs.cullFace));
                currentRs.cullFace = rs.cullFace;
            }
        }

        if (currentRs.frontFace != rs.frontFace) {
            CHECK(glFrontFace(rs.frontFace));
            currentRs.frontFace = rs.frontFace;
        }

        SetDepthBias(rs.depthBias, 0, rs.depthBiasSlop);
        SetLineWidth(rs.lineWidth);
    }

    void CommandContext::SetDepthStencil(const DepthStencilState &ds) {
        auto &currentDS = cache->ds.depth;
        if (currentDS.depthTest != ds.depth.depthTest) {
            SetEnable(ds.depth.depthTest, GL_DEPTH_TEST);
            currentDS.depthTest = ds.depth.depthTest;
        }

        if (currentDS.depthWrite != ds.depth.depthWrite) {
            CHECK(glDepthMask(ds.depth.depthWrite));
            currentDS.depthWrite = ds.depth.depthWrite;
        }

        if (currentDS.depthFunc != ds.depth.depthFunc) {
            CHECK(glDepthFunc(ds.depth.depthFunc));
            currentDS.depthFunc = ds.depth.depthFunc;
        }

        SetDepthBound(ds.depth.minDepth, ds.depth.maxDepth);

        if (cache->ds.stencilTest != ds.stencilTest) {
            SetEnable(ds.stencilTest, GL_STENCIL_TEST);
            cache->ds.stencilTest = ds.stencilTest;
        }

        // front
        SetStencilFunc(StencilFace::FRONT, ds.front.func, ds.front.reference, ds.front.readMask);
        SetStencilWriteMask(StencilFace::FRONT, ds.front.writemask);
        SetStencilOp(StencilFace::FRONT, ds.front.failOp, ds.front.zFailOp, ds.front.zPassOp);

        // back
        SetStencilFunc(StencilFace::BACK, ds.back.func, ds.back.reference, ds.back.readMask);
        SetStencilWriteMask(StencilFace::BACK, ds.back.writemask);
        SetStencilOp(StencilFace::BACK, ds.back.failOp, ds.back.zFailOp, ds.back.zPassOp);
    }

    void CommandContext::CmdBindPipeline(const GraphicsPipelinePtr &pso)
    {
        currentPso = pso;

        auto program = pso->GetProgram();
        if (cache->program != program) {
            cache->program = program;
            CHECK(glUseProgram(program));
        }

        const auto &state = pso->GetGLState();
        cache->primitive = state.primitive;

        SetDepthStencil(state.ds);
        SetRasterizerState(state.rs);
        SetBlendState(state.bs);
    }

    void CommandContext::CmdBindAssembly(const VertexAssemblyPtr &assembly)
    {
        auto vao = assembly->AcquireNativeHandle(static_cast<uint32_t>(type));
        cache->indexType = assembly->GetIndexType();
        if (cache->vao != vao) {
            cache->vao = vao;
            CHECK(glBindVertexArray(vao));
        }
    }

    void CommandContext::CmdSetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        CHECK(glViewport(static_cast<GLint>(viewport->x), static_cast<GLint>(viewport->y),
                   static_cast<GLsizei>(viewport->width), static_cast<GLsizei>(viewport->height)));

        CHECK(glDepthRangef(viewport->minDepth, viewport->maxDepth));
    }

    void CommandContext::CmdSetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        CHECK(glScissor(scissor->offset.x, scissor->offset.y, scissor->extent.width, scissor->extent.height));
    }

    void CommandContext::CmdDrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        CHECK(glDrawElementsInstanced(cache->primitive, indexed.indexCount, cache->indexType, 0, indexed.instanceCount));
    }

    void CommandContext::CmdDrawLinear(const rhi::CmdDrawLinear &linear)
    {
        CHECK(glDrawArraysInstanced(cache->primitive, linear.firstVertex, linear.vertexCount, linear.instanceCount));
    }

    void CommandContext::CmdDrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        CHECK(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle()));

        if (currentPso->GetDevice().GetFeatures().multiDrawIndirect) {
            CHECK(MultiDrawArraysIndirectEXT(cache->primitive, reinterpret_cast<const void*>(offset), count, stride));
        } else {
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t off = offset + i * stride;
                CHECK(glDrawArraysIndirect(cache->primitive, reinterpret_cast<const void*>(off)));
            }
        }
    }

    void CommandContext::CmdDrawIndexedIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        CHECK(glBindBuffer(GL_DRAW_INDIRECT_BUFFER, std::static_pointer_cast<Buffer>(buffer)->GetNativeHandle()));

        if (currentPso->GetDevice().GetFeatures().multiDrawIndirect) {
            CHECK(MultiDrawElementsIndirectEXT(cache->primitive, cache->indexType, reinterpret_cast<const void*>(offset), count, stride));
        } else {
            for (uint32_t i = 0; i < count; ++i) {
                uint32_t off = offset + i * stride;
                CHECK(glDrawElementsIndirect(cache->primitive, cache->indexType, reinterpret_cast<const void*>(off)));
            }
        }
    }

    void CommandContext::CmdEndPass()
    {
        EndPassInternal();
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        cache->drawBuffer = 0;

        CHECK(glBindVertexArray(cache->vao));
        cache->vao        = 0;

        currentFramebuffer = nullptr;
        currentRenderPass  = nullptr;
        currentPso         = nullptr;
        currentSubPassId = 0;
    }

    void CommandContext::Attach(Queue &queue)
    {
        context = queue.GetContext();
        cache = queue.GetCacheState();
        type = queue.GetQueueType();
        sets.clear();
    }
}
