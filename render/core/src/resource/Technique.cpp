//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Technique.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <render/RenderNameHandle.h>
#include <shader/ShaderCompiler.h>
#include <core/archive/FileArchive.h>

namespace sky {

    struct ShaderCacheHeader {
        std::string signature;
        uint32_t version;
    };

    void Technique::SetShader(const ShaderRef &shader)
    {
        shaderData = shader;
        RequestProgram(nullptr);
    }

    RDProgramPtr Technique::RequestProgram(const ShaderPreprocessorPtr &preprocessor)
    {
        auto program = std::make_shared<Program>();

        ShaderCompileOption option = {};
        option.target = ShaderCompileTarget::SPIRV;
        option.preprocessor = preprocessor;

        FillProgramInternal(*program, option);
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

    void GraphicsTechnique::FillProgramInternal(Program &program, const ShaderCompileOption &option)
    {
        rhi::Shader::Descriptor shaderDesc = {};
        {
            ShaderBuildResult result = {};
            shaderData.shaderCollection->BuildAndCacheShader(shaderData.vertOrMeshMain, rhi::ShaderStageFlagBit::VS, option, result);

            shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
            shaderDesc.stage = rhi::ShaderStageFlagBit::VS;

            program.AddShader(RHI::Get()->GetDevice()->CreateShader(shaderDesc));
        }

        {
            ShaderBuildResult result = {};
            shaderData.shaderCollection->BuildAndCacheShader(shaderData.fragmentMain, rhi::ShaderStageFlagBit::FS, option, result);

            shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
            shaderDesc.stage = rhi::ShaderStageFlagBit::FS;

            program.AddShader(RHI::Get()->GetDevice()->CreateShader(shaderDesc));
        }
    }

    void ComputeTechnique::FillProgramInternal(Program &program, const ShaderCompileOption &option)
    {
        ShaderBuildResult result = {};
        shaderData.shaderCollection->BuildAndCacheShader(shaderData.fragmentMain, rhi::ShaderStageFlagBit::CS, option, result);
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
