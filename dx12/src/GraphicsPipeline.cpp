//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/GraphicsPipeline.h>
#include <dx12/Device.h>

namespace sky::dx {

    GraphicsPipeline::GraphicsPipeline(Device &dev) : DevObject(dev)
    {
    }

    GraphicsPipeline::~GraphicsPipeline()
    {
    }

    bool GraphicsPipeline::Init(const Descriptor&)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
        pipelineDesc.pRootSignature;
        pipelineDesc.VS;
        pipelineDesc.PS;
        pipelineDesc.DS;
        pipelineDesc.HS;
        pipelineDesc.GS;
        pipelineDesc.StreamOutput;
        pipelineDesc.BlendState;
        pipelineDesc.SampleMask;
        pipelineDesc.RasterizerState;
        pipelineDesc.DepthStencilState;
        pipelineDesc.InputLayout;
        pipelineDesc.IBStripCutValue;
        pipelineDesc.PrimitiveTopologyType;
        pipelineDesc.NumRenderTargets;
        pipelineDesc.RTVFormats[8];
        pipelineDesc.DSVFormat;
        pipelineDesc.SampleDesc;
        pipelineDesc.NodeMask;
        pipelineDesc.CachedPSO;
        pipelineDesc.Flags;

        if (FAILED(device.GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())))) {
            return false;
        }

        return true;
    }

}