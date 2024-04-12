//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/PipelineLayout.h>
#include <dx12/Device.h>
#include <dx12/DescriptorSetLayout.h>
#include <dx12/Conversion.h>

namespace sky::dx {
    PipelineLayout::PipelineLayout(Device &dev) : DevObject(dev)
    {
    }

    ID3D12RootSignature *PipelineLayout::GetRootSignature()
    {
        return rootSignature.Get();
    }

    bool PipelineLayout::Init(const Descriptor &desc)
    {
        for (const auto &desLayout : desc.layouts) {
            auto &tableMap = std::static_pointer_cast<DescriptorSetLayout>(desLayout)->GetDescriptorTableMap();

            for (auto &[vis, table] : tableMap) {
                D3D12_ROOT_PARAMETER param = {D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE};
                param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param.DescriptorTable = table;
                param.ShaderVisibility = FromRHI(vis);

                parameters.emplace_back(param);
            }
        }

        D3D12_ROOT_SIGNATURE_DESC signatureDesc = {};
        signatureDesc.NumParameters     = static_cast<uint32_t>(parameters.size());
        signatureDesc.pParameters       = parameters.data();
        signatureDesc.NumStaticSamplers = 0;
        signatureDesc.pStaticSamplers   = nullptr;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&signatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), error.GetAddressOf()))) {
            return false;
        }

        if (FAILED(device.GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(),
                                                           signature->GetBufferSize(),
                                                           IID_PPV_ARGS(rootSignature.GetAddressOf())))) {
            return false;
        }

        return true;
    }

}