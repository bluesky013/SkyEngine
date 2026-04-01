//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

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

    private:
        D3D12Device &device;
        ComPtr<ID3D12PipelineState> pso;
    };

    class D3D12ComputePipeline : public ComputePipeline {
    public:
        explicit D3D12ComputePipeline(D3D12Device &dev);
        ~D3D12ComputePipeline() override = default;

        bool Init(const Descriptor &desc, D3D12RootSignature &rootSig);

        ID3D12PipelineState *GetNativeHandle() const { return pso.Get(); }

    private:
        D3D12Device &device;
        ComPtr<ID3D12PipelineState> pso;
    };

} // namespace sky::aurora