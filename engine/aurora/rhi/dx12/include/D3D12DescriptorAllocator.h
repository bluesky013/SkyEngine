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

    // A descriptor range allocation spanning the two CPU-only staging heaps
    // (CBV/SRV/UAV and sampler). Indices are heap-local.
    struct DescriptorAllocation {
        uint32_t cbvSrvUavFirst = 0;
        uint32_t cbvSrvUavCount = 0;
        uint32_t samplerFirst   = 0;
        uint32_t samplerCount   = 0;
    };

    // Global descriptor heap allocator:
    // - two persistent CPU-only staging heaps (CBV/SRV/UAV + sampler) as the
    //   source of truth; ResourceGroup descriptors are carved out here.
    // - a shader-visible ring of `ringSize` heaps per type (one per in-flight
    //   frame), each the same size as the staging heap (offset 1:1). A frame's
    //   descriptors are copied from the staging heap into the current ring heap
    //   at bind time (CopyDescriptorsSimple).
    class D3D12DescriptorAllocator {
    public:
        D3D12DescriptorAllocator() = default;
        ~D3D12DescriptorAllocator() = default;

        bool Init(D3D12Device &device, uint32_t cbvSrvUavSize, uint32_t samplerSize, uint32_t ringSize = 3);

        void BeginFrame(uint32_t frameIndex);

        uint32_t GetCurrentFrame() const { return mCurrentFrame; }

        bool Allocate(uint32_t cbvSrvUavCount, uint32_t samplerCount, DescriptorAllocation &out);
        void Free(const DescriptorAllocation &alloc);

        // CPU-only staging handles (encoder writes here; copy source).
        D3D12_CPU_DESCRIPTOR_HANDLE GetCbvSrvUavCpuHandle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE GetSamplerCpuHandle(uint32_t index) const;

        // Shader-visible handles for the current frame (bind reads here; copy dest).
        D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle(uint32_t index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerGpuHandle(uint32_t index) const;

        // Copy a persistent staging range into the current frame's shader-visible heap.
        void CopyCbvSrvUav(uint32_t first, uint32_t count);
        void CopySampler(uint32_t first, uint32_t count);

        ID3D12DescriptorHeap *GetCbvSrvUavHeap() const;
        ID3D12DescriptorHeap *GetSamplerHeap() const;

    private:
        struct FreeRange {
            uint32_t first = 0;
            uint32_t count = 0;
        };

        static bool AllocateFrom(std::vector<FreeRange> &freeList, uint32_t count, uint32_t &outFirst);
        static void FreeTo(std::vector<FreeRange> &freeList, uint32_t first, uint32_t count);

        D3D12Device *mDevice = nullptr;

        ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap; // CPU-only staging
        ComPtr<ID3D12DescriptorHeap> samplerHeap;   // CPU-only staging

        std::vector<ComPtr<ID3D12DescriptorHeap>> cbvSrvUavRing; // shader-visible, per frame
        std::vector<ComPtr<ID3D12DescriptorHeap>> samplerRing;   // shader-visible, per frame

        uint32_t cbvSrvUavIncrement = 0;
        uint32_t samplerIncrement   = 0;

        uint32_t mCurrentFrame = 0;
        uint32_t mRingSize     = 1;

        std::vector<FreeRange> cbvSrvUavFree;
        std::vector<FreeRange> samplerFree;
    };

} // namespace sky::aurora
