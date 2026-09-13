//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <D3D12ShaderFunction.h>
#include <aurora/rhi/PipelineState.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;
    class D3D12RootSignature;

    class D3D12GraphicsPipeline : public GraphicsPipeline {
    public:
        explicit D3D12GraphicsPipeline(D3D12Device &dev);
        ~D3D12GraphicsPipeline() override = default;

        bool Init(const Descriptor &desc);

        ID3D12PipelineState *GetNativeHandle() const { return pso.Get(); }
        const D3D12RootSignature *GetRootSignature() const { return shader->GetRootSignature(); }

    private:
        D3D12Device &device;
        ComPtr<ID3D12PipelineState> pso;
        CounterPtr<D3D12Shader>     shader;
    };

    class D3D12ComputePipeline : public ComputePipeline {
    public:
        explicit D3D12ComputePipeline(D3D12Device &dev);
        ~D3D12ComputePipeline() override = default;

        bool Init(const Descriptor &desc);

        ID3D12PipelineState *GetNativeHandle() const { return pso.Get(); }
        const D3D12RootSignature *GetRootSignature() const { return shader->GetRootSignature(); }

    private:
        D3D12Device &device;
        ComPtr<ID3D12PipelineState> pso;
        CounterPtr<D3D12Shader>     shader;
    };

} // namespace sky::aurora