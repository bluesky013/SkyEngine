//
// Created on 2026/09/13.
//

#pragma once

#include <D3D12DescriptorAllocator.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/ShaderReflection.h>
#include <d3d12.h>

#include <vector>

namespace sky::aurora {

    class D3D12Device;
    class D3D12Shader;
    class D3D12Buffer;

    class D3D12ResourceGroup : public ResourceGroup {
    public:
        // A dynamic (root CBV / root UAV) binding: the buffer address is
        // rebound at bind time with a per-draw offset; no descriptor is written.
        struct DynamicBinding {
            uint32_t           binding;
            ShaderResourceType type;
            D3D12Buffer       *buffer     = nullptr;
            uint64_t           baseOffset = 0;
        };

        explicit D3D12ResourceGroup(D3D12Device &dev);
        ~D3D12ResourceGroup() override;

        bool Init(const Descriptor &desc);

        void Update(const std::vector<ResourceUpdateInfo> &writes) override;

        D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle() const;

        const std::vector<DynamicBinding> &GetDynamicBindings() const { return dynamicBindings; }

    private:
        D3D12Device                *device = nullptr;
        CounterPtr<D3D12Shader>     shader; // keeps reflection source alive
        std::vector<ShaderResource> setResources;
        std::vector<uint32_t>       cbvSrvUavOffsets; // per resource
        std::vector<uint32_t>       samplerOffsets;   // per resource
        uint32_t                    cbvSrvUavCount = 0;
        uint32_t                    samplerCount   = 0;
        DescriptorAllocation        allocation;
        std::vector<DynamicBinding> dynamicBindings;
    };

} // namespace sky::aurora
