//
// Created on 2026/09/13.
//

#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    // A shader-visible descriptor range allocation spanning the two D3D12
    // descriptor heaps (CBV/SRV/UAV and sampler). Indices are heap-local.
    struct DescriptorAllocation {
        uint32_t cbvSrvUavFirst = 0;
        uint32_t cbvSrvUavCount = 0;
        uint32_t samplerFirst   = 0;
        uint32_t samplerCount   = 0;
    };

    // Global descriptor heap allocator backed by two shader-visible heaps
    // (CBV/SRV/UAV + sampler) with a free-list per heap. D3D12 has no pool
    // concept, so ResourceGroup descriptors are carved out of these heaps.
    class D3D12DescriptorAllocator {
    public:
        D3D12DescriptorAllocator() = default;
        ~D3D12DescriptorAllocator() = default;

        bool Init(D3D12Device &device, uint32_t cbvSrvUavSize, uint32_t samplerSize);

        bool Allocate(uint32_t cbvSrvUavCount, uint32_t samplerCount, DescriptorAllocation &out);
        void Free(const DescriptorAllocation &alloc);

        ID3D12DescriptorHeap *GetCbvSrvUavHeap() const { return cbvSrvUavHeap.Get(); }
        ID3D12DescriptorHeap *GetSamplerHeap() const { return samplerHeap.Get(); }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCbvSrvUavCpuHandle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpuHandle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle(uint32_t index) const;

    private:
        struct FreeRange {
            uint32_t first = 0;
            uint32_t count = 0;
        };

        static bool AllocateFrom(std::vector<FreeRange> &freeList, uint32_t count, uint32_t &outFirst);
        static void FreeTo(std::vector<FreeRange> &freeList, uint32_t first, uint32_t count);

        ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
        ComPtr<ID3D12DescriptorHeap> samplerHeap;

        uint32_t cbvSrvUavIncrement = 0;
        uint32_t samplerIncrement   = 0;

        std::vector<FreeRange> cbvSrvUavFree;
        std::vector<FreeRange> samplerFree;
    };

} // namespace sky::aurora
