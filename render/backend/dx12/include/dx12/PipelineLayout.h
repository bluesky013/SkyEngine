//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <vector>
#include <rhi/PipelineLayout.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class PipelineLayout : public rhi::PipelineLayout, public DevObject {
    public:
        explicit PipelineLayout(Device &dev);
        ~PipelineLayout() override = default;

        ID3D12RootSignature *GetRootSignature();

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        ComPtr<ID3D12RootSignature> rootSignature;
        std::vector<D3D12_ROOT_PARAMETER> parameters;
    };

}