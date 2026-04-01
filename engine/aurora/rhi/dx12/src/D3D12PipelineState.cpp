//
// Created by Zach Lee on 2026/3/31.
//

#include "D3D12ShaderFunction.h"

#include <D3D12Device.h>
#include <D3D12PipelineState.h>
#include <D3D12RootSignature.h>
#include <D3D12ShaderFunction.h>
#include <D3D12Conversion.h>
#include <core/logger/Logger.h>

namespace sky::aurora {

    static const char *TAG = "D3D12PipelineState";

    // ---- D3D12GraphicsPipeline ----

    D3D12GraphicsPipeline::D3D12GraphicsPipeline(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12GraphicsPipeline::Init(const Descriptor &desc)
    {
        if (desc.state == nullptr || desc.shader == nullptr) {
            LOG_E(TAG, "graphics pipeline descriptor missing state or shader");
            return false;
        }

        const auto &state = *desc.state;
        auto *d3dShader = static_cast<D3D12Shader *>(desc.shader);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = d3dShader->GetRootSignature()->GetNativeHandle();

        // shader bytecode
        psoDesc.VS = d3dShader->GetVSByteCode();
        psoDesc.PS = d3dShader->GetPSByteCode();

        // input layout (empty for now, vertex pulling or mesh shaders)
        psoDesc.InputLayout.pInputElementDescs = nullptr;
        psoDesc.InputLayout.NumElements        = 0;

        // rasterizer
        auto &rs = psoDesc.RasterizerState;
        rs.FillMode            = FromPolygonMode(state.rasterState.polygonMode);
        rs.CullMode            = FromCullMode(state.rasterState.cullMode);
        rs.FrontCounterClockwise = state.rasterState.frontFace == FrontFace::CCW ? TRUE : FALSE;
        rs.DepthBias             = static_cast<INT>(state.rasterState.depthBiasConstantFactor);
        rs.DepthBiasClamp        = state.rasterState.depthBiasClamp;
        rs.SlopeScaledDepthBias  = state.rasterState.depthBiasSlopeFactor;
        rs.DepthClipEnable       = state.rasterState.depthClipEnable ? TRUE : FALSE;
        rs.MultisampleEnable     = FALSE;
        rs.AntialiasedLineEnable = FALSE;
        rs.ForcedSampleCount     = 0;
        rs.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // depth stencil
        auto &ds = psoDesc.DepthStencilState;
        ds.DepthEnable    = state.depthStencil.depthTest ? TRUE : FALSE;
        ds.DepthWriteMask = state.depthStencil.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc      = FromCompareOp(state.depthStencil.compareOp);
        ds.StencilEnable  = state.depthStencil.stencilTest ? TRUE : FALSE;
        ds.StencilReadMask  = static_cast<UINT8>(state.depthStencil.front.compareMask);
        ds.StencilWriteMask = static_cast<UINT8>(state.depthStencil.front.writeMask);
        ds.FrontFace = FromStencilState(state.depthStencil.front);
        ds.BackFace  = FromStencilState(state.depthStencil.back);

        // blend state
        auto &bs = psoDesc.BlendState;
        bs.AlphaToCoverageEnable  = state.multiSample.alphaToCoverage ? TRUE : FALSE;
        bs.IndependentBlendEnable = FALSE;

        const auto &fmt = desc.format;
        psoDesc.NumRenderTargets = fmt.numColors;

        for (UINT i = 0; i < fmt.numColors && i < 8; ++i) {
            psoDesc.RTVFormats[i] = FromPixelFormat(fmt.colors[i]);

            auto &rt = bs.RenderTarget[i];
            if (i < state.blendStates.size()) {
                const auto &src     = state.blendStates[i];
                rt.BlendEnable      = src.blendEn ? TRUE : FALSE;
                rt.LogicOpEnable    = FALSE;
                rt.SrcBlend         = FromBlendFactor(src.srcColor);
                rt.DestBlend        = FromBlendFactor(src.dstColor);
                rt.BlendOp          = FromBlendOp(src.colorBlendOp);
                rt.SrcBlendAlpha    = FromBlendFactor(src.srcAlpha);
                rt.DestBlendAlpha   = FromBlendFactor(src.dstAlpha);
                rt.BlendOpAlpha     = FromBlendOp(src.alphaBlendOp);
                rt.RenderTargetWriteMask = src.writeMask;
            } else {
                rt.BlendEnable           = FALSE;
                rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            }
        }

        psoDesc.DSVFormat = FromPixelFormat(fmt.depthStencil);

        // topology
        psoDesc.PrimitiveTopologyType = FromPrimitiveTopology(state.inputAssembly.topology);

        // multi-sample
        psoDesc.SampleMask = 0xFFFFFFFFU;
        psoDesc.SampleDesc.Count   = static_cast<UINT>(state.multiSample.sampleCount);
        psoDesc.SampleDesc.Quality = 0;

        // misc
        psoDesc.NodeMask = 0;
        psoDesc.Flags    = D3D12_PIPELINE_STATE_FLAG_NONE;

        // D3D12 view instancing for multiview
        D3D12_VIEW_INSTANCING_DESC viewInstancing = {};
        std::vector<D3D12_VIEW_INSTANCE_LOCATION> viewLocations;
        if (fmt.viewMask != 0) {
            for (uint32_t bit = 0; bit < 32; ++bit) {
                if (fmt.viewMask & (1u << bit)) {
                    D3D12_VIEW_INSTANCE_LOCATION loc = {};
                    loc.ViewportArrayIndex     = bit;
                    loc.RenderTargetArrayIndex = bit;
                    viewLocations.push_back(loc);
                }
            }
            viewInstancing.ViewInstanceCount = static_cast<UINT>(viewLocations.size());
            viewInstancing.pViewInstanceLocations = viewLocations.data();
            viewInstancing.Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
        }

        const HRESULT hr = device.GetNativeHandle()->CreateGraphicsPipelineState(
            &psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create graphics pipeline state, hr=0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        return true;
    }

    // ---- D3D12ComputePipeline ----

    D3D12ComputePipeline::D3D12ComputePipeline(D3D12Device &dev)
        : device(dev)
    {
    }

    bool D3D12ComputePipeline::Init(const Descriptor &desc, D3D12RootSignature &rootSig)
    {
        if (desc.cs == nullptr) {
            LOG_E(TAG, "compute pipeline descriptor missing compute shader");
            return false;
        }

        auto *d3dShader = static_cast<D3D12Shader *>(desc.cs);

        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = rootSig.GetNativeHandle();
        psoDesc.CS             = d3dShader->GetCSByteCode();
        psoDesc.NodeMask       = 0;
        psoDesc.Flags          = D3D12_PIPELINE_STATE_FLAG_NONE;

        const HRESULT hr = device.GetNativeHandle()->CreateComputePipelineState(
            &psoDesc, IID_PPV_ARGS(pso.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create compute pipeline state, hr=0x%08X", static_cast<unsigned>(hr));
            return false;
        }

        return true;
    }

} // namespace sky::aurora