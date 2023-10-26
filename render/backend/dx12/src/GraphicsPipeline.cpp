//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/GraphicsPipeline.h>
#include <dx12/Device.h>

namespace sky::dx {

    GraphicsPipeline::GraphicsPipeline(Device &dev) : DevObject(dev)
    {
    }

    bool GraphicsPipeline::Init(const Descriptor &desc)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};

        // shader
        pipelineDesc.VS;
        pipelineDesc.PS;

        // vertex desc
        pipelineDesc.StreamOutput;
        pipelineDesc.InputLayout;
        pipelineDesc.IBStripCutValue;

        // pipeline stats
        pipelineDesc.BlendState;
        pipelineDesc.RasterizerState;
        pipelineDesc.DepthStencilState;
        pipelineDesc.PrimitiveTopologyType;

        // render pass
        pipelineDesc.NumRenderTargets;
        pipelineDesc.RTVFormats[8];
        pipelineDesc.DSVFormat;

        // sample
        pipelineDesc.SampleMask;
        pipelineDesc.SampleDesc = {static_cast<UINT>(desc.state.multiSample.sampleCount), 0};

        pipelineDesc.NodeMask;
        pipelineDesc.CachedPSO;
        pipelineDesc.Flags;

        return !FAILED(device.GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
    }

}