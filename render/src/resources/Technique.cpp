//
// Created by Zach Lee on 2022/5/28.
//


#include <render/resources/Technique.h>
#include <render/DriverManager.h>
#include <core/hash/Hash.h>

namespace sky {

    void GraphicsTechnique::SetShaderTable(RDGfxShaderTablePtr shaders)
    {
        table = shaders;
    }

    void GraphicsTechnique::SetRenderPass(RDPassPtr p, uint32_t subPass)
    {
        pass = p;
        subPassIndex = subPass;
    }

    drv::GraphicsPipelinePtr GraphicsTechnique::AcquirePso(drv::VertexInputPtr& vertexInput)
    {
        return AcquirePso(vertexInput, {});
    }

    drv::GraphicsPipelinePtr GraphicsTechnique::AcquirePso(drv::VertexInputPtr& vi, drv::ShaderOptionPtr option)
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

        drv::GraphicsPipeline::Program program;
        table->FillProgram(program);
        program.shaderOption = option;

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.renderPass = pass->GetRenderPass();
        psoDesc.program = &program;
        psoDesc.state = &pipelineState;
        psoDesc.vertexInput = vi;
        psoDesc.pipelineLayout = table->GetPipelineLayout();
        psoDesc.subPassIndex = subPassIndex;

        auto device = DriverManager::Get()->GetDevice();
        auto pso = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);
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

    RDGfxShaderTablePtr GraphicsTechnique::GetShaderTable() const
    {
        return table;
    }
}