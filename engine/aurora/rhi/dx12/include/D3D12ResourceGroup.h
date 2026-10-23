//
// Created on 2026/09/13.
//

#pragma once

#include <D3D12Buffer.h>
#include <D3D12DescriptorAllocator.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/ShaderReflection.h>
#include <d3d12.h>

#include <map>
#include <vector>

namespace sky::aurora {

    class D3D12Device;
    class D3D12Shader;
    class D3D12Buffer;
    class D3D12DescriptorEncoder;

    class D3D12ResourceGroup : public ResourceGroup {
    public:
        // Keep dynamic backing alive while its address is rebound with a per-draw offset.
        struct DynamicBinding {
            uint32_t                binding;
            ShaderResource          resource;
            CounterPtr<D3D12Buffer> buffer;
            uint64_t                baseOffset = 0;
            uint64_t                range      = 0;
        };

        explicit D3D12ResourceGroup(D3D12Device &dev);
        ~D3D12ResourceGroup() override;
        const Device *GetDevice() const noexcept override;

        bool Init(const Descriptor &desc);

        std::unique_ptr<DescriptorEncoder> CreateEncoder() override;

        // Copy the persistent CPU-only staging range into the current frame's
        // shader-visible heap (when dirty or first bind of this frame).
        void EnsureFrameCopy();

        D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle() const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle() const;

        const std::vector<DynamicBinding> &GetDynamicBindings() const
        {
            return dynamicBindings;
        }

    private:
        friend class D3D12DescriptorEncoder;

        D3D12Device                                                   *device = nullptr;
        CounterPtr<D3D12Shader>                                        shader; // keeps reflection source alive
        std::vector<ShaderResource>                                    setResources;
        std::vector<uint32_t>                                          cbvSrvUavOffsets; // per resource
        std::vector<uint32_t>                                          samplerOffsets;   // per resource
        uint32_t                                                       cbvSrvUavCount = 0;
        uint32_t                                                       samplerCount   = 0;
        DescriptorAllocation                                           allocation;
        std::vector<DynamicBinding>                                    dynamicBindings;
        std::map<std::pair<uint32_t, uint32_t>, CounterPtr<RefObject>> pins;
        bool                                                           mDirty       = false;
        uint32_t                                                       mCopiedFrame = INVALID_INDEX;
    };

} // namespace sky::aurora
