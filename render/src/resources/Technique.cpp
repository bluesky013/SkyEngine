//
// Created by Zach Lee on 2022/5/28.
//


#include <render/resources/Technique.h>
#include <render/DriverManager.h>

namespace sky {

    void GraphicsTechnique::SetShaderTable(RDGfxShaderTablePtr shaders)
    {
        table = shaders;
        table->FillProgram(program);
    }

    void GraphicsTechnique::SetRenderPass(RDPassPtr p)
    {
        pass = p;
    }

    void GraphicsTechnique::InitRHI()
    {
        if (!pso) {
            return;
        }

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.renderPass = pass->GetRenderPass();
        psoDesc.program = &program;
        psoDesc.state = &pipelineState;
        psoDesc.pipelineLayout = table->GetPipelineLayout();

        auto device = DriverManager::Get()->GetDevice();
        pso = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);
    }

    bool GraphicsTechnique::IsValid() const
    {
        return !!pso;
    }

}