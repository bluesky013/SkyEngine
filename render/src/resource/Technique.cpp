//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Technique.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/RenderNameHandle.h>

namespace sky {

    void Technique::AddShader(const sky::RDShaderPtr &shader)
    {
        shaders.emplace_back(shader);
    }

    RDProgramPtr Technique::RequestProgram(const std::string &key)
    {
        auto program = std::make_shared<Program>();
        for (auto &shader : shaders) {
            auto variant = shader->GetVariant(key);
            if (!variant) {
                return nullptr;
            }
            program->AddShader(variant);
        }
        program->BuildPipelineLayout();
        programs.emplace(key, program);
        return program;
    }

    void GraphicsTechnique::SetDepthStencil(const rhi::DepthStencil &ds)
    {
        state.depthStencil = ds;
    }

    void GraphicsTechnique::SetRasterState(const rhi::RasterState &rs)
    {
        state.rasterState = rs;
    }

    void GraphicsTechnique::SetBlendState(const std::vector<rhi::BlendState> &blends)
    {
        state.blendStates = blends;
    }

    void GraphicsTechnique::SetRasterTag(const std::string &tag)
    {
        rasterID = RenderNameHandle::Get()->GetOrRegisterName(tag);
    }

    void GraphicsTechnique::SetVertexLayout(const std::string &key)
    {
        vertexDesc = Renderer::Get()->GetVertexDescLibrary()->FindVertexDesc(key);
    }

    rhi::GraphicsPipelinePtr GraphicsTechnique::BuildPso(GraphicsTechnique &tech,
                                      const rhi::RenderPassPtr &pass,
                                      uint32_t subPassID,
                                      const std::string &programKey)
    {
        auto program = tech.RequestProgram(programKey);
        if (!program) {
            return nullptr;
        }

        const auto &shaders = program->GetShaders();

        rhi::GraphicsPipeline::Descriptor descriptor = {};
        descriptor.state = tech.state;
        descriptor.renderPass = pass;
        descriptor.subPassIndex = subPassID;
        descriptor.pipelineLayout = program->GetPipelineLayout();
        descriptor.vertexInput = tech.vertexDesc;
        descriptor.vs = shaders[0]->GetShader();
        descriptor.fs = shaders[1]->GetShader();

        if (descriptor.state.blendStates.empty()) {
            descriptor.state.blendStates.resize(pass->GetColors().size());
        }

        return RHI::Get()->GetDevice()->CreateGraphicsPipeline(descriptor);
    }
} // namespace sky
