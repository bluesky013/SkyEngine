//
// Created on 2026/09/13.
//

#pragma once

#include <D3D12DescriptorAllocator.h>
#include <aurora/rhi/ResourceGroup.h>
#include <d3d12.h>

namespace sky::aurora {

    class D3D12Device;
    class D3D12ResourceGroupLayout;

    class D3D12ResourceGroup : public ResourceGroup {
    public:
        explicit D3D12ResourceGroup(D3D12Device &dev);
        ~D3D12ResourceGroup() override;

        bool Init(const Descriptor &desc);

        void Update(const std::vector<ResourceUpdateInfo> &writes) override;

        D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle() const;

    private:
        D3D12Device              *device = nullptr;
        D3D12ResourceGroupLayout *layout = nullptr;
        DescriptorAllocation      allocation;
    };

} // namespace sky::aurora
