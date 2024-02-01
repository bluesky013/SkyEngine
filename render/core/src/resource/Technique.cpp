//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Technique.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/RenderNameHandle.h>
#include <shader/ShaderCompiler.h>

namespace sky {

    void Technique::SetShader(const ShaderRef &shader)
    {
        shaderData = shader;

        const auto &source = shader.shaderCollection->RequestSource();
        std::vector<std::vector<uint32_t>> out;
        sl::ShaderReflection reflection = {};
        sl::ShaderCompiler::Get()->BuildSpirV(source, {
                                                          {shader.vertOrMeshMain, rhi::ShaderStageFlagBit::VS},
                                                          {shader.fragmentMain, rhi::ShaderStageFlagBit::FS}
                                                      }, out, reflection);

        sl::ShaderCompiler::Get()->BuildDXIL(source);
    }

    RDProgramPtr Technique::RequestProgram(const ShaderPreprocessor &preprocessor)
    {
        auto iter = cachePrograms.find(preprocessor.GetHash());
        if (iter != cachePrograms.end()) {
            return iter->second;
        }

        auto program = std::make_shared<Program>();
//        program->SetShader(shaderCollection);
//        program->BuildPipelineLayout();
//        cachePrograms.emplace(preprocessor.BuildSource(), program);
        return program;
    }

    RDProgramPtr Technique::RequestProgram()
    {
        auto program = std::make_shared<Program>();
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
        vertexDescKey = key;
        vertexDesc = Renderer::Get()->GetVertexDescLibrary()->FindVertexDesc(key);
    }

//    rhi::GraphicsPipelinePtr GraphicsTechnique::BuildPso(GraphicsTechnique &tech,
//                                      const rhi::RenderPassPtr &pass,
//                                      uint32_t subPassID,
//                                      const std::string &programKey)
//    {
//        rhi::GraphicsPipeline::Descriptor descriptor = {};
//        descriptor.state = tech.state;
//        descriptor.state.multiSample.sampleCount = pass->GetSamplerCount();
//        descriptor.renderPass = pass;
//        descriptor.subPassIndex = subPassID;
//        descriptor.pipelineLayout = program->GetPipelineLayout();
//        descriptor.vertexInput = tech.vertexDesc;
//        descriptor.vs = shaders[0]->GetShader();
//        descriptor.fs = shaders[1]->GetShader();
//
//        if (descriptor.state.blendStates.empty()) {
//            descriptor.state.blendStates.resize(pass->GetColors().size());
//        }
//
//        return RHI::Get()->GetDevice()->CreateGraphicsPipeline(descriptor);
//    }
} // namespace sky
