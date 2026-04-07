//
// Created on 2026/04/07.
//

#include <GLESEncoder.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <GLESBuffer.h>
#include <GLESImage.h>
#include <GLESPipelineState.h>
#include <GLESConversion.h>

namespace sky::aurora {

    // ---- GLESGraphicsEncoder ----

    GLESGraphicsEncoder::GLESGraphicsEncoder(GLESDevice &device)
        : device(device)
    {
    }

    void GLESGraphicsEncoder::BeginRendering(const RenderingInfo &info)
    {
        // Create transient FBO and attach color/depth targets
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        GLenum drawBuffers[MAX_COLOR_ATTACHMENTS];
        for (uint32_t i = 0; i < info.numColors; ++i) {
            auto *img = static_cast<GLESImage *>(info.colors[i].image);
            if (img != nullptr) {
                if (img->IsRenderBuffer()) {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER, img->GetNativeHandle());
                } else {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, img->GetTarget(), img->GetNativeHandle(), 0);
                }
            }
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        if (info.numColors > 0) {
            glDrawBuffers(static_cast<GLsizei>(info.numColors), drawBuffers);
        }

        // Depth/stencil attachment
        if (info.depthStencil.image != nullptr) {
            auto *dsImg = static_cast<GLESImage *>(info.depthStencil.image);
            GLenum attach = GL_DEPTH_ATTACHMENT;
            if (dsImg->IsRenderBuffer()) {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attach, GL_RENDERBUFFER, dsImg->GetNativeHandle());
            } else {
                glFramebufferTexture2D(GL_FRAMEBUFFER, attach, dsImg->GetTarget(), dsImg->GetNativeHandle(), 0);
            }
        }

        // Set viewport/scissor from render area
        glViewport(info.renderArea.offset.x, info.renderArea.offset.y,
                   static_cast<GLsizei>(info.renderArea.extent.width),
                   static_cast<GLsizei>(info.renderArea.extent.height));
        glScissor(info.renderArea.offset.x, info.renderArea.offset.y,
                  static_cast<GLsizei>(info.renderArea.extent.width),
                  static_cast<GLsizei>(info.renderArea.extent.height));

        // Handle clears
        GLbitfield clearMask = 0;
        for (uint32_t i = 0; i < info.numColors; ++i) {
            if (info.colors[i].loadOp == LoadOp::CLEAR) {
                const auto &c = info.colors[i].clearValue.color;
                glClearColor(c.float32[0], c.float32[1], c.float32[2], c.float32[3]);
                clearMask |= GL_COLOR_BUFFER_BIT;
                break; // glClearColor applies to all targets
            }
        }
        if (info.depthStencil.image != nullptr && info.depthStencil.depthLoadOp == LoadOp::CLEAR) {
            glClearDepthf(info.depthStencil.clearValue.depthStencil.depth);
            clearMask |= GL_DEPTH_BUFFER_BIT;
        }
        if (info.depthStencil.image != nullptr && info.depthStencil.stencilLoadOp == LoadOp::CLEAR) {
            glClearStencil(static_cast<GLint>(info.depthStencil.clearValue.depthStencil.stencil));
            clearMask |= GL_STENCIL_BUFFER_BIT;
        }
        if (clearMask != 0) {
            glClear(clearMask);
        }
    }

    void GLESGraphicsEncoder::EndRendering()
    {
        if (fbo != 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fbo);
            fbo = 0;
        }
    }

    void GLESGraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto *glesPso = static_cast<GLESGraphicsPipeline *>(pso);
        glesPso->Bind();
    }

    void GLESGraphicsEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement UBO/texture binding once ResourceGroup is mapped
    }

    void GLESGraphicsEncoder::BindVertexBuffers(uint32_t /*firstBinding*/, uint32_t count, const BufferView *views)
    {
        // In GLES, vertex buffers are bound via VAO + glVertexAttribPointer.
        // This sets the buffer binding points for later use.
        for (uint32_t i = 0; i < count; ++i) {
            auto *buf = static_cast<GLESBuffer *>(views[i].buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buf->GetNativeHandle());
            // Vertex attribute setup deferred to pipeline bind / VAO
        }
    }

    void GLESGraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        auto *buf = static_cast<GLESBuffer *>(buffer);
        boundIndexBuffer = buf->GetNativeHandle();
        indexBufferOffset = offset;
        currentIndexType = FromIndexType(type);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, boundIndexBuffer);
    }

    void GLESGraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        if (count > 0) {
            const auto &vp = viewports[0];
            glViewport(static_cast<GLint>(vp.x), static_cast<GLint>(vp.y),
                       static_cast<GLsizei>(vp.width), static_cast<GLsizei>(vp.height));
        }
    }

    void GLESGraphicsEncoder::SetScissor(uint32_t count, const Rect2D *scissors)
    {
        if (count > 0) {
            const auto &sc = scissors[0];
            glScissor(sc.offset.x, sc.offset.y,
                      static_cast<GLsizei>(sc.extent.width),
                      static_cast<GLsizei>(sc.extent.height));
        }
    }

    void GLESGraphicsEncoder::Draw(const CmdDrawLinear &cmd)
    {
        if (cmd.instanceCount <= 1) {
            glDrawArrays(currentTopology, static_cast<GLint>(cmd.firstVertex), static_cast<GLsizei>(cmd.vertexCount));
        } else {
            glDrawArraysInstanced(currentTopology, static_cast<GLint>(cmd.firstVertex),
                                  static_cast<GLsizei>(cmd.vertexCount), static_cast<GLsizei>(cmd.instanceCount));
        }
    }

    void GLESGraphicsEncoder::DrawIndexed(const CmdDrawIndexed &cmd)
    {
        uint32_t indexSize = (currentIndexType == GL_UNSIGNED_SHORT) ? 2 : 4;
        const void *offset = reinterpret_cast<const void *>(
            static_cast<uintptr_t>(indexBufferOffset + cmd.firstIndex * indexSize));

        if (cmd.instanceCount <= 1) {
            glDrawElements(currentTopology, static_cast<GLsizei>(cmd.indexCount), currentIndexType, offset);
        } else {
            glDrawElementsInstanced(currentTopology, static_cast<GLsizei>(cmd.indexCount), currentIndexType,
                                    offset, static_cast<GLsizei>(cmd.instanceCount));
        }
    }

    void GLESGraphicsEncoder::DrawIndirect(Buffer * /*buffer*/, uint64_t /*offset*/, uint32_t /*drawCount*/, uint32_t /*stride*/)
    {
        // GL_DRAW_INDIRECT_BUFFER path - requires GLES 3.1+ extension
        // TODO: implement with glDrawArraysIndirect
    }

    void GLESGraphicsEncoder::DrawIndexedIndirect(Buffer * /*buffer*/, uint64_t /*offset*/, uint32_t /*drawCount*/, uint32_t /*stride*/)
    {
        // TODO: implement with glDrawElementsIndirect
    }

    // ---- GLESComputeEncoder ----

    GLESComputeEncoder::GLESComputeEncoder(GLESDevice &device)
        : device(device)
    {
    }

    void GLESComputeEncoder::BindPipeline(ComputePipeline * /*pso*/)
    {
        // GLES compute uses programs; pipeline binding is a UseProgram call
        // TODO: implement once ComputePipeline stores the GL program
    }

    void GLESComputeEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement SSBO/UBO binding
    }

    void GLESComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        glDispatchCompute(groupX, groupY, groupZ);
    }

    void GLESComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        auto *buf = static_cast<GLESBuffer *>(buffer);
        glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, buf->GetNativeHandle());
        glDispatchComputeIndirect(static_cast<GLintptr>(offset));
    }

    // ---- GLESBlitEncoder ----

    GLESBlitEncoder::GLESBlitEncoder(GLESDevice &device)
        : device(device)
    {
        glGenFramebuffers(1, &readFbo);
        glGenFramebuffers(1, &writeFbo);
    }

    void GLESBlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        auto *srcBuf = static_cast<GLESBuffer *>(src);
        auto *dstBuf = static_cast<GLESBuffer *>(dst);

        glBindBuffer(GL_COPY_READ_BUFFER, srcBuf->GetNativeHandle());
        glBindBuffer(GL_COPY_WRITE_BUFFER, dstBuf->GetNativeHandle());
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                            static_cast<GLintptr>(srcOffset),
                            static_cast<GLintptr>(dstOffset),
                            static_cast<GLsizeiptr>(size));
    }

    void GLESBlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcBuf = static_cast<GLESBuffer *>(src);
        auto *dstImg = static_cast<GLESImage *>(dst);
        const auto &fmt = FromPixelFormat(dstImg->GetPixelFormat());

        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, srcBuf->GetNativeHandle());
        for (const auto &region : regions) {
            const void *offset = reinterpret_cast<const void *>(static_cast<uintptr_t>(region.bufferOffset));
            if (dstImg->GetTarget() == GL_TEXTURE_3D || dstImg->GetTarget() == GL_TEXTURE_2D_ARRAY) {
                glTexSubImage3D(dstImg->GetTarget(), static_cast<GLint>(region.subRange.level),
                                region.imageOffset.x, region.imageOffset.y, region.imageOffset.z,
                                static_cast<GLsizei>(region.imageExtent.width),
                                static_cast<GLsizei>(region.imageExtent.height),
                                static_cast<GLsizei>(region.imageExtent.depth),
                                fmt.format, fmt.type, offset);
            } else {
                glTexSubImage2D(dstImg->GetTarget(), static_cast<GLint>(region.subRange.level),
                                region.imageOffset.x, region.imageOffset.y,
                                static_cast<GLsizei>(region.imageExtent.width),
                                static_cast<GLsizei>(region.imageExtent.height),
                                fmt.format, fmt.type, offset);
            }
        }
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    }

    void GLESBlitEncoder::CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions)
    {
        auto *srcImg = static_cast<GLESImage *>(src);
        auto *dstBuf = static_cast<GLESBuffer *>(dst);
        const auto &fmt = FromPixelFormat(srcImg->GetPixelFormat());

        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, dstBuf->GetNativeHandle());

        for (const auto &region : regions) {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   srcImg->GetTarget(), srcImg->GetNativeHandle(),
                                   static_cast<GLint>(region.subRange.level));
            void *offset = reinterpret_cast<void *>(static_cast<uintptr_t>(region.bufferOffset));
            glReadPixels(region.imageOffset.x, region.imageOffset.y,
                         static_cast<GLsizei>(region.imageExtent.width),
                         static_cast<GLsizei>(region.imageExtent.height),
                         fmt.format, fmt.type, offset);
        }

        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    }

    void GLESBlitEncoder::BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter)
    {
        auto *srcImg = static_cast<GLESImage *>(src);
        auto *dstImg = static_cast<GLESImage *>(dst);
        GLenum glFilter = (filter == Filter::LINEAR) ? GL_LINEAR : GL_NEAREST;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFbo);

        for (const auto &region : regions) {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   srcImg->GetTarget(), srcImg->GetNativeHandle(),
                                   static_cast<GLint>(region.srcRange.level));
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   dstImg->GetTarget(), dstImg->GetNativeHandle(),
                                   static_cast<GLint>(region.dstRange.level));

            glBlitFramebuffer(
                region.srcOffsets[0].x, region.srcOffsets[0].y,
                region.srcOffsets[1].x, region.srcOffsets[1].y,
                region.dstOffsets[0].x, region.dstOffsets[0].y,
                region.dstOffsets[1].x, region.dstOffsets[1].y,
                GL_COLOR_BUFFER_BIT, glFilter);
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

    void GLESBlitEncoder::ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions)
    {
        auto *srcImg = static_cast<GLESImage *>(src);
        auto *dstImg = static_cast<GLESImage *>(dst);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, writeFbo);

        for (const auto &region : regions) {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   srcImg->GetTarget(), srcImg->GetNativeHandle(),
                                   static_cast<GLint>(region.srcRange.level));
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   dstImg->GetTarget(), dstImg->GetNativeHandle(),
                                   static_cast<GLint>(region.dstRange.level));

            glBlitFramebuffer(
                region.srcOffset.x, region.srcOffset.y,
                region.srcOffset.x + static_cast<GLint>(region.extent.width),
                region.srcOffset.y + static_cast<GLint>(region.extent.height),
                region.dstOffset.x, region.dstOffset.y,
                region.dstOffset.x + static_cast<GLint>(region.extent.width),
                region.dstOffset.y + static_cast<GLint>(region.extent.height),
                GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    }

} // namespace sky::aurora
