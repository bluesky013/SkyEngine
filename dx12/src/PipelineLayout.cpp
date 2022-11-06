//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/PipelineLayout.h>
#include <dx12/Device.h>

namespace sky::dx {
    PipelineLayout::PipelineLayout(Device &dev) : DevObject(dev)
    {
    }

    PipelineLayout::~PipelineLayout()
    {
    }

    const ID3D12RootSignature *PipelineLayout::GetRootSignature() const
    {
        return rootSignature.Get();
    }

    bool PipelineLayout::Init(const Descriptor &desc)
    {
        D3D12_ROOT_SIGNATURE_DESC  signatureDesc = {};
        signatureDesc.NumParameters;
        signatureDesc.pParameters;
        signatureDesc.NumStaticSamplers;
        signatureDesc.pStaticSamplers;
        signatureDesc.Flags;

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3D12SerializeRootSignature(&signatureDesc,
                                               D3D_ROOT_SIGNATURE_VERSION_1,
                                               signature.GetAddressOf(),
                                               error.GetAddressOf()))) {
            return false;
        }

        if (FAILED(device.GetDevice()->CreateRootSignature(0,
                                                           signature->GetBufferPointer(),
                                                           signature->GetBufferSize(),
                                                           IID_PPV_ARGS(rootSignature.GetAddressOf())))) {
            return false;
        }

        return true;
    }

}