//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/GraphicsPipeline.h>
#include <dx12/RenderPass.h>
#include <dx12/Device.h>
#include <dx12/Conversion.h>
#include <dx12/VertexInput.h>
#include <dx12/PipelineLayout.h>
#include <core/logger/Logger.h>

namespace sky::dx {
    static const char* TAG = "D3D12PSO";

    GraphicsPipeline::GraphicsPipeline(Device &dev) : DevObject(dev)
    {
    }

    bool GraphicsPipeline::Init(const Descriptor &desc)
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};

        // root signature
        pipelineDesc.pRootSignature = std::static_pointer_cast<PipelineLayout>(desc.pipelineLayout)->GetRootSignature();

        // shader
        pipelineDesc.VS = std::static_pointer_cast<Shader>(desc.vs)->GetByteCode();
        pipelineDesc.PS = std::static_pointer_cast<Shader>(desc.fs)->GetByteCode();

        // vertex desc
        pipelineDesc.InputLayout = std::static_pointer_cast<VertexInput>(desc.vertexInput)->GetVertexDesc();

        // stream out
        pipelineDesc.StreamOutput = {};

        // strip cut value
        pipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

        // rasterizer state
        pipelineDesc.RasterizerState.FillMode = FromRHI(desc.state.rasterState.polygonMode);
        pipelineDesc.RasterizerState.CullMode = FromRHI(desc.state.rasterState.cullMode);
        pipelineDesc.RasterizerState.FrontCounterClockwise = desc.state.rasterState.frontFace == rhi::FrontFace::CW ? FALSE : TRUE;
        pipelineDesc.RasterizerState.DepthBias = static_cast<int>(desc.state.rasterState.depthBiasConstantFactor);
        pipelineDesc.RasterizerState.DepthBiasClamp       = desc.state.rasterState.depthBiasClamp;
        pipelineDesc.RasterizerState.SlopeScaledDepthBias = desc.state.rasterState.depthBiasSlopeFactor;
        pipelineDesc.RasterizerState.DepthClipEnable      = static_cast<BOOL>(desc.state.rasterState.depthClipEnable);
        pipelineDesc.RasterizerState.MultisampleEnable     = FALSE;
        pipelineDesc.RasterizerState.ForcedSampleCount     = 0;
        pipelineDesc.RasterizerState.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pipelineDesc.RasterizerState.AntialiasedLineEnable = FALSE;

        // depth stencil state
        pipelineDesc.DepthStencilState.DepthEnable = static_cast<BOOL>(desc.state.depthStencil.depthTest);
        pipelineDesc.DepthStencilState.DepthWriteMask = desc.state.depthStencil.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        pipelineDesc.DepthStencilState.DepthFunc = FromRHI(desc.state.depthStencil.compareOp);
        pipelineDesc.DepthStencilState.StencilEnable = static_cast<BOOL>(desc.state.depthStencil.stencilTest);

        // stencil
        if ((desc.state.depthStencil.front.compareMask != desc.state.depthStencil.back.compareMask) ||
            (desc.state.depthStencil.front.writeMask != desc.state.depthStencil.back.writeMask)) {
            LOG_W(TAG, "compareMask and writeMask with different values in stencil front && back is not supported, use front value");
        }

        pipelineDesc.DepthStencilState.StencilReadMask = desc.state.depthStencil.front.compareMask;
        pipelineDesc.DepthStencilState.StencilWriteMask = desc.state.depthStencil.front.writeMask;

        pipelineDesc.DepthStencilState.FrontFace.StencilFailOp = FromRHI(desc.state.depthStencil.front.failOp);
        pipelineDesc.DepthStencilState.FrontFace.StencilDepthFailOp = FromRHI(desc.state.depthStencil.front.failOp);
        pipelineDesc.DepthStencilState.FrontFace.StencilPassOp = FromRHI(desc.state.depthStencil.front.failOp);
        pipelineDesc.DepthStencilState.FrontFace.StencilFunc = FromRHI(desc.state.depthStencil.front.compareOp);

        pipelineDesc.DepthStencilState.BackFace.StencilFailOp = FromRHI(desc.state.depthStencil.back.failOp);
        pipelineDesc.DepthStencilState.BackFace.StencilDepthFailOp = FromRHI(desc.state.depthStencil.back.failOp);
        pipelineDesc.DepthStencilState.BackFace.StencilPassOp = FromRHI(desc.state.depthStencil.back.failOp);
        pipelineDesc.DepthStencilState.BackFace.StencilFunc = FromRHI(desc.state.depthStencil.back.compareOp);

        pipelineDesc.PrimitiveTopologyType = FromRHI(desc.state.inputAssembly.topology);

        // render pass
        auto pass = std::static_pointer_cast<RenderPass>(desc.renderPass);
        const auto &colors = pass->GetColorFormats();
        pipelineDesc.NumRenderTargets = static_cast<UINT>(colors.size());

        for (UINT i = 0; i < pipelineDesc.NumRenderTargets; ++i) {
            pipelineDesc.RTVFormats[i] = colors[i];
            auto &blendDesc = pipelineDesc.BlendState.RenderTarget[i];
            const auto &rhiBlendDesc = desc.state.blendStates[i];
            blendDesc.BlendEnable = static_cast<BOOL>(rhiBlendDesc.blendEn);
            blendDesc.LogicOpEnable = FALSE;
            blendDesc.LogicOp = D3D12_LOGIC_OP_CLEAR;

            blendDesc.SrcBlend  = FromRHI(rhiBlendDesc.srcColor);
            blendDesc.DestBlend = FromRHI(rhiBlendDesc.dstColor);
            blendDesc.BlendOp   = FromRHI(rhiBlendDesc.colorBlendOp);
            blendDesc.SrcBlendAlpha  = FromRHI(rhiBlendDesc.srcAlpha);
            blendDesc.DestBlendAlpha = FromRHI(rhiBlendDesc.dstAlpha);
            blendDesc.BlendOpAlpha   = FromRHI(rhiBlendDesc.alphaBlendOp);
            blendDesc.RenderTargetWriteMask = rhiBlendDesc.writeMask;
        }
        pipelineDesc.BlendState.AlphaToCoverageEnable = static_cast<BOOL>(desc.state.multiSample.alphaToCoverage);
        pipelineDesc.BlendState.IndependentBlendEnable = FALSE;

        pipelineDesc.DSVFormat = pass->GetDSFormat();

        // sample
        pipelineDesc.SampleMask = 0xFFFFFFFFU;
        pipelineDesc.SampleDesc = {static_cast<UINT>(desc.state.multiSample.sampleCount), 0};

        pipelineDesc.NodeMask = 1;
        pipelineDesc.CachedPSO.CachedBlobSizeInBytes = 0;
        pipelineDesc.CachedPSO.pCachedBlob = nullptr;

        pipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        return !FAILED(device.GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(pipelineState.GetAddressOf())));
    }

}