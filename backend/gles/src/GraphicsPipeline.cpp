//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/GraphicsPipeline.h>
#include <gles/Shader.h>
#include <gles/Core.h>
#include <gles/Conversion.h>
#include <gles/PipelineLayout.h>
#include <core/logger/Logger.h>
#include <unordered_set>

static const char* TAG = "GLES";

namespace sky::gles {

    static std::unordered_set<GLenum> SAMPLER_TYPE = {
        GL_SAMPLER_2D,
        GL_SAMPLER_3D,
        GL_SAMPLER_CUBE,
        GL_SAMPLER_2D_SHADOW,
        GL_SAMPLER_2D_ARRAY,
        GL_SAMPLER_2D_ARRAY_SHADOW,
        GL_SAMPLER_CUBE_SHADOW,
        GL_INT_SAMPLER_2D,
        GL_INT_SAMPLER_3D,
        GL_INT_SAMPLER_CUBE,
        GL_INT_SAMPLER_2D_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_2D,
        GL_UNSIGNED_INT_SAMPLER_3D,
        GL_UNSIGNED_INT_SAMPLER_CUBE,
        GL_UNSIGNED_INT_SAMPLER_2D_ARRAY
    };

    bool GraphicsPipeline::InitProgram(const Descriptor &desc)
    {
        program = glCreateProgram();

        auto attachShader = [this](const rhi::ShaderPtr &shader)
        {
            ShaderPtr esShader = std::static_pointer_cast<Shader>(shader);
            CHECK(glAttachShader(program, esShader->GetNativeHandle()));
        };
        attachShader(desc.vs);
        attachShader(desc.fs);

        CHECK(glLinkProgram(program));
        GLint status = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &status);
        if (status != GL_TRUE) {
            GLint logSize = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logSize);
            std::unique_ptr<GLchar[]> data = std::make_unique<GLchar[]>(logSize + 1);
            CHECK(glGetProgramInfoLog(program, logSize, nullptr, data.get()));

            LOG_E(TAG, "link program failed. error%s", data.get());
            return false;
        }
        return true;
    }

    void GraphicsPipeline::InitVertexInput()
    {
        GLint activeAttributes = 0;
        GLint maxLength = 0;
        CHECK(glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &activeAttributes));
        CHECK(glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength));

        attributes.resize(activeAttributes);
        for (uint32_t i = 0; i < activeAttributes; ++i) {
            auto &attribute = attributes[i];

            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            attribute.name.resize(maxLength, 0);
            CHECK(glGetActiveAttrib(program, i, maxLength, &length, &size, &type, attribute.name.data()));

            GLint location = glGetAttribLocation(program, attribute.name.c_str());
            attribute.location = location;
        }

    }

    void GraphicsPipeline::InitDescriptorIndices(const Descriptor &desc)
    {
        if (!desc.pipelineLayout) {
            return;
        }

        auto pipelineLayout = std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout);
        auto &descLayouts = pipelineLayout->GetLayouts();

        for (uint32_t set = 0, offset1 = 0; set < descLayouts.size(); ++set) {
            descriptorOffsets.emplace_back(offset1);
            auto &layout = descLayouts[set];
            auto &bindings = layout->GetBindings();

            uint32_t offset2 = 0;
            for (auto &binding : bindings) {
                descriptorIndices.resize(descriptorIndices.size() + binding.count);
                auto *indicesBase = &descriptorIndices[offset1 + offset2];

                if (binding.type == rhi::DescriptorType::UNIFORM_BUFFER ||
                    binding.type == rhi::DescriptorType::UNIFORM_BUFFER_DYNAMIC) {
                    GLuint blockIndex = glGetProgramResourceIndex(program, GL_UNIFORM_BLOCK, binding.name.c_str());
                    for (uint32_t i = 0; i < binding.count && blockIndex != ~(0U); ++i) {
                        auto &desIndex = indicesBase[i];
                        GLenum prop = GL_BUFFER_BINDING;
                        GLint index = 0;
                        CHECK(glGetProgramResourceiv(program, GL_UNIFORM_BLOCK, blockIndex, 1, &prop, 1, nullptr, &index));
                        desIndex.binding = index;
                    }

                } else if (binding.type == rhi::DescriptorType::STORAGE_BUFFER ||
                           binding.type == rhi::DescriptorType::STORAGE_BUFFER_DYNAMIC) {
                    GLuint bufferIndex = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, binding.name.c_str());
                    for (uint32_t i = 0; i < binding.count && bufferIndex != ~(0U); ++i) {
                        auto &desIndex = indicesBase[i];
                        GLenum prop = GL_BUFFER_BINDING;
                        GLint index = 0;
                        CHECK(glGetProgramResourceiv(program, GL_SHADER_STORAGE_BLOCK, bufferIndex, 1, &prop, 1, nullptr, &index));
                        desIndex.binding = index;
                    }
                } else if (binding.type == rhi::DescriptorType::SAMPLED_IMAGE ||
                           binding.type == rhi::DescriptorType::COMBINED_IMAGE_SAMPLER ||
                           binding.type == rhi::DescriptorType::STORAGE_IMAGE) {
                    GLuint loc = glGetUniformLocation(program, binding.name.c_str());
                    std::vector<GLint> units;
                    for (uint32_t i = 0; i < binding.count && loc != GL_INVALID_INDEX; ++i) {
                        auto &desIndex = indicesBase[i];
                        desIndex.unit = loc + i;
                        units.emplace_back(loc + i);
                    }
                    if (!units.empty()) {
                        CHECK(glUniform1iv(loc, static_cast<GLsizei>(units.size()), units.data()));
                        units.clear();
                    }
                }
                offset2 += binding.count;
            }
            offset1 += layout->GetDescriptorCount();
        }
    }

    void GraphicsPipeline::InitGLState(const Descriptor &desc)
    {
        // depthStencil;
        state.ds.depth.depthTest = desc.state.depthStencil.depthTest;
        state.ds.depth.depthWrite   = desc.state.depthStencil.depthWrite;
        state.ds.depth.minDepth    = desc.state.depthStencil.minDepth;
        state.ds.depth.maxDepth    = desc.state.depthStencil.maxDepth;
        state.ds.depth.depthFunc   = FromRHI(desc.state.depthStencil.compareOp);
        state.ds.stencilTest = desc.state.depthStencil.stencilTest;
        state.ds.front       = FromRHI(desc.state.depthStencil.front);
        state.ds.back        = FromRHI(desc.state.depthStencil.back);

        // multiSample;

        // vertexDesc;
        state.primitive = FromRHI(desc.state.inputAssembly.topology);

        /*
         * rasterState, unsupported
         *     depthClampEnable
         *     depthBiasClamp
         *     polygonMode
         */
//        state.rs.rasterizerDiscard = desc.state.rasterState.rasterizerDiscardEnable;
//        state.rs.polygonOffsetEn = desc.state.rasterState.depthBiasEnable;
//        state.rs.polygonConstant = desc.state.rasterState.depthBiasConstantFactor;
//        state.rs.polygonUnits    = desc.state.rasterState.depthBiasSlopeFactor;
        state.rs.lineWidth       = desc.state.rasterState.lineWidth;

        state.rs.cullingEn   = !!desc.state.rasterState.cullMode;
        state.rs.cullFace     = FromRHI(desc.state.rasterState.cullMode);
        state.rs.frontFace       = FromRHI(desc.state.rasterState.frontFace);

        // blendStates;
        state.bs.target.reserve(desc.state.blendStates.size());
        for (auto &blend : desc.state.blendStates) {
            state.bs.target.emplace_back(FromRHI(blend));
        }
    }

    bool GraphicsPipeline::Init(const Descriptor &desc)
    {
        if (!InitProgram(desc)) {
            return false;
        }
        CHECK(glUseProgram(program));
        InitVertexInput();
        InitDescriptorIndices(desc);
        InitGLState(desc);
        CHECK(glUseProgram(0));

        pipelineLayout = std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout);
        return true;
    }

}