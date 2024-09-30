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

    static ShaderCompileTarget GetCompileTarget()
    {
        auto api = RHI::Get()->GetBackend();
        switch (api) {
        case rhi::API::METAL:
            return ShaderCompileTarget::MSL;
        case rhi::API::DX12:
            return ShaderCompileTarget::DXIL;
        case rhi::API::VULKAN:
        default:
            return ShaderCompileTarget::SPIRV;
        };
    }

    struct ShaderCacheHeader {
        std::string signature;
        uint32_t version;
    };

    void Technique::SetShader(const ShaderRef &shader)
    {
        shaderData = shader;
    }

    RDProgramPtr Technique::RequestProgram(const ShaderOptionPtr &shaderOption)
    {
        uint32_t hash = shaderOption ? shaderOption->GetHash() : 0;
        auto iter = programCache.find(hash);
        if (iter != programCache.end()) {
            return iter->second;
        }

        if (!shaderData.shaderCollection) {
            return {};
        }

        auto *program = new Program();
        program->SetName(shaderData.shaderCollection->GetName());

        ShaderCompileOption option = {};
        option.target = GetCompileTarget();
        option.option = shaderOption;

        FillProgramInternal(*program, option);
        programCache[hash] = program;
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

    void GraphicsTechnique::SetVertexFlag(RenderVertexFlagBit bit, const std::string &macro)
    {
        vertexFlags[bit] = macro;
    }

    void GraphicsTechnique::Process(RenderVertexFlags flags, const ShaderOptionPtr &option)
    {
        for (auto &val : preCompiledFlags) {
            option->SetValue(val, true);
        }

        for (auto &flag : vertexFlags) {
            if (flags.TestBit(flag.first)) {
                option->SetValue(flag.second, true);
            }
        }
    }

    void GraphicsTechnique::FillProgramInternal(Program &program, const ShaderCompileOption &option)
    {
        auto *device = RHI::Get()->GetDevice();
        rhi::Shader::Descriptor shaderDesc = {};
        {
            ShaderBuildResult result = {};
            shaderData.shaderCollection->BuildAndCacheShader(shaderData.vertOrMeshMain, rhi::ShaderStageFlagBit::VS, option, result);

            shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
            shaderDesc.stage = rhi::ShaderStageFlagBit::VS;

            auto shader = device->CreateShader(shaderDesc);
            shader->SetEntry(shaderData.vertOrMeshMain);
            program.AddShader(shader);
            program.MergeReflection(std::move(result.reflection));
        }

        {
            ShaderBuildResult result = {};
            shaderData.shaderCollection->BuildAndCacheShader(shaderData.fragmentMain, rhi::ShaderStageFlagBit::FS, option, result);

            shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
            shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
            shaderDesc.stage = rhi::ShaderStageFlagBit::FS;

            auto shader = device->CreateShader(shaderDesc);
            shader->SetEntry(shaderData.fragmentMain);
            program.AddShader(shader);
            program.MergeReflection(std::move(result.reflection));
        }
        program.Build();
    }

    void ComputeTechnique::FillProgramInternal(Program &program, const ShaderCompileOption &option)
    {
        ShaderBuildResult result = {};
        shaderData.shaderCollection->BuildAndCacheShader(shaderData.fragmentMain, rhi::ShaderStageFlagBit::CS, option, result);

        rhi::Shader::Descriptor shaderDesc = {};
        shaderDesc.data = reinterpret_cast<const uint8_t *>(result.data.data());
        shaderDesc.size = static_cast<uint32_t>(result.data.size()) * sizeof(uint32_t);
        shaderDesc.stage = rhi::ShaderStageFlagBit::CS;

        program.AddShader(RHI::Get()->GetDevice()->CreateShader(shaderDesc));
        program.MergeReflection(std::move(result.reflection));
    }

    rhi::GraphicsPipelinePtr GraphicsTechnique::BuildPso(const RDProgramPtr &program,
                                      const rhi::PipelineState &state,
                                      const rhi::VertexInputPtr &vertexDesc,
                                      const rhi::RenderPassPtr &pass,
                                      uint32_t subPassID)
    {
        const auto &shaders = program->GetShaders();

        rhi::GraphicsPipeline::Descriptor descriptor = {};
        descriptor.state = state;
        descriptor.state.multiSample.sampleCount = pass->GetSamplerCount();
        descriptor.renderPass = pass;
        descriptor.subPassIndex = subPassID;
        descriptor.pipelineLayout = program->GetPipelineLayout();
        descriptor.vertexInput = vertexDesc;
        descriptor.vs = shaders[0];
        descriptor.fs = shaders[1];

        if (descriptor.state.blendStates.empty()) {
            descriptor.state.blendStates.resize(pass->GetColors().size());
        }

        return RHI::Get()->GetDevice()->CreateGraphicsPipeline(descriptor);
    }
} // namespace sky
