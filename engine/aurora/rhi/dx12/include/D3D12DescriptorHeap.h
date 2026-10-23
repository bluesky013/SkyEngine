//
// D3D12 tier2 bindless DescriptorHeap (Shader Model 6.6 ResourceDescriptorHeap).
// Two shader-visible backing heaps (resource = CBV/SRV/UAV, sampler) with
// per-type index allocation. Descriptor writes are immediate (D3D12 has no
// batched flush), issued through a heap-bound DescriptorEncoder.
//

#pragma once

#include <aurora/rhi/DescriptorHeap.h>

#include <d3d12.h>
#include <memory>
#include <vector>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;
    class D3D12DescriptorHeap;

    // Heap bindings are allocation-relative indices; untyped buffer writes are raw SRVs.
    class D3D12HeapDescriptorEncoder : public DescriptorEncoder {
    public:
        D3D12HeapDescriptorEncoder(D3D12Device                      &device,
                                   ID3D12DescriptorHeap             *resourceHeap,
                                   ID3D12DescriptorHeap             *samplerHeap,
                                   uint32_t                          resourceIncrement,
                                   uint32_t                          samplerIncrement,
                                   const DescriptorHeap::Allocation &allocation);

        bool WriteBuffer(uint32_t index, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool WriteImage(uint32_t index, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool WriteSampler(uint32_t index, Sampler *sampler, uint32_t arrayElement = 0) override;
        bool End() override
        {
            return !failed;
        }

        bool WriteBufferCBV(uint32_t index, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0);
        bool WriteBufferUAV(uint32_t index, Buffer *buffer, uint32_t arrayElement = 0);
        bool WriteImageUAV(uint32_t index, Image *image, uint32_t arrayElement = 0);

    private:
        bool                        Fail(const char *message);
        bool                        failed = false;
        D3D12_CPU_DESCRIPTOR_HANDLE ResourceHandle(uint32_t index) const;
        D3D12_CPU_DESCRIPTOR_HANDLE SamplerHandle(uint32_t index) const;

        ID3D12Device              *native            = nullptr;
        D3D12Device               *owner             = nullptr;
        ID3D12DescriptorHeap      *resourceHeap      = nullptr;
        ID3D12DescriptorHeap      *samplerHeap       = nullptr;
        uint32_t                   resourceIncrement = 0;
        uint32_t                   samplerIncrement  = 0;
        DescriptorHeap::Allocation allocation;
    };

    class D3D12DescriptorHeap : public DescriptorHeap {
    public:
        bool Init(D3D12Device &device, const Descriptor &desc);

        Allocation Allocate(const Descriptor &desc) override;
        void       Free(const Allocation &allocation) override;
        // D3D12 writes descriptors immediately via CreateHeapEncoder; this is a
        // no-op kept for interface symmetry.
        void Update(const Allocation &allocation, DescriptorEncoder &encoder) override;

        std::unique_ptr<DescriptorEncoder> CreateHeapEncoder(const Allocation &allocation);

        ID3D12DescriptorHeap *GetResourceHeap() const
        {
            return resourceHeap.Get();
        }
        ID3D12DescriptorHeap *GetSamplerHeap() const
        {
            return samplerHeap.Get();
        }

    private:
        struct FreeRange {
            uint32_t first = 0;
            uint32_t count = 0;
        };

        static bool AllocateFrom(std::vector<FreeRange> &freeList, uint32_t count, uint32_t &outFirst);
        static void FreeTo(std::vector<FreeRange> &freeList, uint32_t first, uint32_t count);

        D3D12Device *device = nullptr;

        ComPtr<ID3D12DescriptorHeap> resourceHeap;
        ComPtr<ID3D12DescriptorHeap> samplerHeap;

        uint32_t resourceIncrement = 0;
        uint32_t samplerIncrement  = 0;

        uint32_t maxTextures = 0;
        uint32_t maxBuffers  = 0;

        std::vector<FreeRange> texFree;
        std::vector<FreeRange> bufFree;
        std::vector<FreeRange> smpFree;
    };

} // namespace sky::aurora
