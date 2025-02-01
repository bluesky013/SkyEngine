//
// Created by Zach Lee on 2023/8/31.
//

#include <render/resource/Technique.h>
#include <render/RHI.h>
#include <render/Renderer.h>
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

    void Technique::SetShader(const ShaderRef &shader_)
    {
        shaderData = shader_;

        auto *source = ShaderCacheManager::Get()->FetchSource(shader_.shaderName, RHI::Get()->GetShaderTarget());
        if (source != nullptr) {
            shader = new ShaderCollection(shaderData.shaderName, *source);
        }
    }

    void Technique::SetOption(const Name& name, uint8_t val, ShaderVariantKey &variantKey)
    {
        if (shader) {
            shader->SetOption(name, val, variantKey);
        }
    }

    RDProgramPtr Technique::RequestProgram(const ShaderVariantKey &key)
    {
        return shader ? FillProgramInternal(key) : nullptr;
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

    void GraphicsTechnique::SetRasterTag(const Name &tag)
    {
        rasterID = tag;
    }

    void GraphicsTechnique::AddVertexFlag(RenderVertexFlagBit bit, const Name &key)
    {
        vertexFlags[bit] = key;
    }

    void GraphicsTechnique::ProcessVertexVariantKey(RenderVertexFlags flags, ShaderVariantKey &key)
    {
        for (auto &flag : vertexFlags) {
            shader->SetOption(flag.second, flags.TestBit(flag.first), key);
        }
    }

    RDProgramPtr GraphicsTechnique::FillProgramInternal(const ShaderVariantKey &key)
    {
        std::vector<std::pair<Name, rhi::ShaderStageFlagBit>> stages = {
            {Name(shaderData.vertOrMeshMain.c_str()), rhi::ShaderStageFlagBit::VS},
            {Name(shaderData.fragmentMain.c_str()), rhi::ShaderStageFlagBit::FS}
        };
        return shader->AcquireShaderBinary(key, stages);
    }

    RDProgramPtr ComputeTechnique::FillProgramInternal(const ShaderVariantKey &key)
    {
        std::vector<std::pair<Name, rhi::ShaderStageFlagBit>> stages = {
            {Name(shaderData.objectOrCSMain.c_str()), rhi::ShaderStageFlagBit::CS},
        };
        return shader->AcquireShaderBinary(key, stages);
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
