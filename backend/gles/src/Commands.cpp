//
// Created by Zach Lee on 2023/2/5.
//

#include <gles/CommandContext.h>
#include <gles/Core.h>
#include <gles/Queue.h>
#include <gles/GraphicsPipeline.h>

namespace sky::gles {

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
        }
        return 0;
    }

    static GLenum GetTextureTarget(const ImageViewPtr &imageView)
    {
        auto &viewInfo = imageView->GetViewDesc();
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
        clearValues = values;
        clearCount = count;

        auto &ext = currentFramebuffer->GetExtent();
        CHECK(glViewport(0, 0, ext.width, ext.height));
        CHECK(glScissor(0, 0, ext.width, ext.height));

        BeginPassInternal();
    }

    void CommandContext::CmdNextPass()
    {
        ++currentSubPassId;
        BeginPassInternal();
    }

    void CommandContext::BeginPassInternal()
    {
        auto &fbs    = currentFramebuffer->GetNativeHandles();
        SKY_ASSERT(currentSubPassId < fbs.size());

        auto fb   = fbs[currentSubPassId];
        if (fb.surface) {
            if (fb.surface->GetSurface() != context->GetCurrentSurface()) {
                context->MakeCurrent(*fb.surface);
            }
        }

        auto clear = clearValues[0];
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo));
        CHECK(glClear(GL_COLOR_BUFFER_BIT));
        CHECK(glClearColor(clear.color.float32[0], clear.color.float32[1], clear.color.float32[2], clear.color.float32[3]));
        cache->drawBuffer = fb.fbo;
    }

    void CommandContext::CmdBindDescriptorSet(uint32_t setId, const DescriptorSetPtr &set, uint32_t dynamicCount, uint32_t *dynamicOffsets)
    {
        SKY_ASSERT(setId >= MAX_SET_ID);
        if (sets.empty()) {
            sets.resize(MAX_SET_ID);
        }
        sets[setId] = set;

        auto &descriptorOffsets = currentPso->GetDescriptorOffsets();
        auto &descriptorIndices = currentPso->GetDescriptorIndices();

        auto pipelineLayout = currentPso->GetPipelineLayout();
        auto &setLayouts = pipelineLayout->GetLayouts();

        auto &descriptorSetLayout = setLayouts[setId];
        auto &bindings = descriptorSetLayout->GetBindings();
        auto &bindingMap = descriptorSetLayout->GetBindingMap();

        uint32_t dynamicIndex = 0;
        for (auto &binding : bindings) {
            auto offsetToSet            = bindingMap.at(binding.binding);
            auto offsetToPipelineLayout = descriptorOffsets[setId];

            uint32_t *dynamicOffsetPtr = nullptr;
            if (IsDynamicBuffer(binding.type) && dynamicIndex < dynamicCount) {
                dynamicOffsetPtr = &dynamicOffsets[dynamicIndex];
                dynamicIndex += binding.count;
            }

            auto *descriptorIndexBase = &descriptorIndices[offsetToSet + offsetToPipelineLayout];
            auto *descriptorBase      = &set->GetDescriptors()[offsetToSet];

            for (uint32_t i = 0; i < binding.count; ++i) {
                auto &descriptor      = descriptorBase[i];
                auto &descriptorIndex = descriptorIndexBase[i];
                if (IsBufferType(binding.type) && CheckBuffer(descriptor)) {
                    uint32_t dynamicOffset = dynamicOffsetPtr != nullptr ? dynamicOffsetPtr[i] : 0;
                    auto &bufferView = descriptor.buffer.view;
                    auto &viewInfo = bufferView->GetViewDesc();
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

    void CommandContext::CmdBindPipeline(const GraphicsPipelinePtr &pso)
    {
        currentPso = pso;

        auto program = pso->GetProgram();
        if (cache->program != program) {
            cache->program = program;
            CHECK(glUseProgram(program));
        }

        auto &state = pso->GetGLState();
        cache->primitive = state.primitive;


    }

    void CommandContext::CmdBindAssembly(const VertexAssemblyPtr &assembly)
    {
        auto vao = assembly->GetNativeHandle();
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
    }

    void CommandContext::CmdDrawLinear(const rhi::CmdDrawLinear &linear)
    {
        CHECK(glDrawArraysInstanced(cache->primitive, linear.firstVertex, linear.vertexCount, linear.instanceCount));
    }

    void CommandContext::CmdDrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
    }

    void CommandContext::CmdEndPass()
    {
        currentFramebuffer = nullptr;
        currentPso         = nullptr;
        currentSubPassId = 0;
    }

    void CommandContext::Attach(Queue &queue)
    {
        context = queue.GetContext();
        cache = queue.GetCacheState();
        sets.clear();
    }
}