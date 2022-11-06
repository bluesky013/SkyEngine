//
// Created by Zach Lee on 2022/5/28.
//

#include <core/hash/Hash.h>
#include <framework/asset/AssetManager.h>
#include <render/RHIManager.h>
#include <render/resources/Technique.h>

namespace sky {

    void GraphicsTechnique::SetShaderTable(const RDGfxShaderTablePtr &shaders)
    {
        table = shaders;
    }

    void GraphicsTechnique::SetRenderPass(const RDPassPtr &p, uint32_t subPass)
    {
        pass         = p;
        subPassIndex = subPass;
        pass->ValidatePipelineState(pipelineState, subPass);
    }

    vk::GraphicsPipelinePtr GraphicsTechnique::AcquirePso(const vk::VertexInputPtr &vertexInput)
    {
        return AcquirePso(vertexInput, {});
    }

    vk::GraphicsPipelinePtr GraphicsTechnique::AcquirePso(const vk::VertexInputPtr &vi, const vk::ShaderOptionPtr &option)
    {
        uint32_t hash = 0;
        HashCombine32(hash, vi->GetHash());
        if (option) {
            HashCombine32(hash, option->GetHash());
        }

        auto iter = psoCache.find(hash);
        if (iter != psoCache.end()) {
            return iter->second;
        }

        vk::GraphicsPipeline::Program program;
        table->FillProgram(program);
        program.shaderOption = option;

        vk::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.renderPass                        = pass->GetRenderPass();
        psoDesc.program                           = &program;
        psoDesc.state                             = &pipelineState;
        psoDesc.vertexInput                       = vi;
        psoDesc.pipelineLayout                    = table->GetPipelineLayout();
        psoDesc.subPassIndex                      = subPassIndex;

        auto device = RHIManager::Get()->GetDevice();
        auto pso    = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
        psoCache.emplace(hash, pso);
        return pso;
    }

    void GraphicsTechnique::SetViewTag(uint32_t tag)
    {
        viewTag = tag;
    }

    void GraphicsTechnique::SetDrawTag(uint32_t tag)
    {
        drawTag = tag;
    }

    uint32_t GraphicsTechnique::GetViewTag() const
    {
        return viewTag;
    }

    uint32_t GraphicsTechnique::GetDrawTag() const
    {
        return drawTag;
    }

    const RDGfxShaderTablePtr &GraphicsTechnique::GetShaderTable() const
    {
        return table;
    }

    vk::DescriptorSetBinderPtr GraphicsTechnique::CreateSetBinder() const
    {
        auto layout = table->GetPipelineLayout();
        auto res    = std::make_shared<vk::DescriptorSetBinder>();
        res->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        res->SetPipelineLayout(layout);
        return res;
    }

    void GraphicsTechnique::SetDepthTestEn(bool enable)
    {
        pipelineState.depthStencil.depthTestEnable = enable;
    }

    void GraphicsTechnique::SetDepthWriteEn(bool enable)
    {
        pipelineState.depthStencil.depthWriteEnable = enable;
    }

    void GraphicsTechnique::SetDepthStencilState(const vk::GraphicsPipeline::DepthStencilState &ds)
    {
        pipelineState.depthStencil = ds;
    }

    void GraphicsTechnique::SetBlendState(const vk::GraphicsPipeline::ColorBlend &blends)
    {
        pipelineState.blends = blends;
    }

    void GraphicsTechnique::SetRasterState(const vk::GraphicsPipeline::Raster &raster)
    {
        pipelineState.raster = raster;
    }

    vk::GraphicsPipeline::State &GraphicsTechnique::GetState()
    {
        return pipelineState;
    }

    std::shared_ptr<GraphicsTechnique> GraphicsTechnique::CreateFromData(const GfxTechniqueAssetData &data)
    {
        auto gfxTech = std::make_shared<GraphicsTechnique>();
        auto gfxShaderTable = std::make_shared<GraphicsShaderTable>();
        if (data.vs) {
            gfxShaderTable->SetVS(data.vs->CreateInstance());
        }
        if (data.fs) {
            gfxShaderTable->SetFS(data.fs->CreateInstance());
        }
        gfxShaderTable->InitRHI();
        gfxTech->SetShaderTable(gfxShaderTable);
        gfxTech->SetDepthStencilState(data.depthStencilState);
        gfxTech->SetBlendState(data.blends);
        gfxTech->SetRasterState(data.raster);
        gfxTech->SetDrawTag(data.drawTag);
        gfxTech->SetViewTag(data.viewTag);
        return gfxTech;
    }

    void GfxTechniqueAssetData::InitShader(const Uuid &id, ShaderAssetPtr &asset)
    {
        asset = AssetManager::Get()->LoadAsset<Shader>(assetPathMap[id]);
    }

} // namespace sky
