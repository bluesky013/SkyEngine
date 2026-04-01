//
// Created by Zach Lee on 2026/3/31.
//

#pragma once

#include <aurora/rhi/Shader.h>
#include <D3D12RootSignature.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12ShaderFunction : public ShaderFunction {
    public:
        explicit D3D12ShaderFunction(D3D12Device &dev);
        ~D3D12ShaderFunction() override = default;

        bool Init(const Descriptor &desc);

        D3D12_SHADER_BYTECODE GetByteCode() const;

    private:
        D3D12Device &device;
        CounterPtr<ShaderDataProvider> dataProvider;
    };

    class D3D12Shader : public Shader {
    public:
        explicit D3D12Shader(D3D12Device &dev);
        ~D3D12Shader() override = default;

        bool Init(const Descriptor &desc);

        D3D12_SHADER_BYTECODE GetVSByteCode() const;
        D3D12_SHADER_BYTECODE GetPSByteCode() const;
        D3D12_SHADER_BYTECODE GetCSByteCode() const;

        const D3D12RootSignature* GetRootSignature() const { return rootSignature.Get(); }
    private:
        D3D12Device &device;
        ShaderFunctionPtr vs;
        ShaderFunctionPtr psOrCs; // used for both PS and CS since they are mutually exclusive

        CounterPtr<D3D12RootSignature> rootSignature;
    };

} // namespace sky::aurora