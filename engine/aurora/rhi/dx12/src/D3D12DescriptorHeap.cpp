//
// D3D12 tier2 bindless DescriptorHeap implementation.
//

#include <D3D12DescriptorHeap.h>

#include <D3D12Buffer.h>
#include <D3D12Device.h>
#include <D3D12Image.h>
#include <D3D12Sampler.h>

#include <core/logger/Logger.h>

#include <algorithm>
#include <utility>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    D3D12HeapDescriptorEncoder::D3D12HeapDescriptorEncoder(D3D12Device &device,
                                                           ID3D12DescriptorHeap *inResourceHeap,
                                                           ID3D12DescriptorHeap *inSamplerHeap,
                                                           uint32_t inResourceIncrement,
                                                           uint32_t inSamplerIncrement,
                                                           const DescriptorHeap::Allocation &inAllocation)
        : native(device.GetNativeHandle())
        , resourceHeap(inResourceHeap)
        , samplerHeap(inSamplerHeap)
        , resourceIncrement(inResourceIncrement)
        , samplerIncrement(inSamplerIncrement)
        , allocation(inAllocation)
    {
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12HeapDescriptorEncoder::ResourceHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = resourceHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * resourceIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12HeapDescriptorEncoder::SamplerHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * samplerIncrement;
        return handle;
    }

    void D3D12HeapDescriptorEncoder::WriteBuffer(uint32_t index, Buffer *buffer, uint64_t /*offset*/, uint64_t /*range*/,
                                                 uint32_t arrayElement)
    {
        auto *buf = static_cast<D3D12Buffer *>(buffer);
        if (buf == nullptr) {
            return;
        }
        buf->CreateSRV(ResourceHandle(allocation.bufFirst + index + arrayElement));
    }

    void D3D12HeapDescriptorEncoder::WriteBufferCBV(uint32_t index, Buffer *buffer, uint64_t offset, uint64_t range,
                                                    uint32_t arrayElement)
    {
        auto *buf = static_cast<D3D12Buffer *>(buffer);
        if (buf == nullptr) {
            return;
        }
        buf->CreateCBV(ResourceHandle(allocation.bufFirst + index + arrayElement), offset, range);
    }

    void D3D12HeapDescriptorEncoder::WriteBufferUAV(uint32_t index, Buffer *buffer, uint32_t arrayElement)
    {
        auto *buf = static_cast<D3D12Buffer *>(buffer);
        if (buf == nullptr) {
            return;
        }
        buf->CreateUAV(ResourceHandle(allocation.bufFirst + index + arrayElement));
    }

    void D3D12HeapDescriptorEncoder::WriteImage(uint32_t index, Image *image, ImageLayout /*layout*/,
                                                uint32_t arrayElement)
    {
        auto *img = static_cast<D3D12Image *>(image);
        if (img == nullptr) {
            return;
        }
        img->CreateSRV(ResourceHandle(allocation.texFirst + index + arrayElement));
    }

    void D3D12HeapDescriptorEncoder::WriteImageUAV(uint32_t index, Image *image, uint32_t arrayElement)
    {
        auto *img = static_cast<D3D12Image *>(image);
        if (img == nullptr) {
            return;
        }
        img->CreateUAV(ResourceHandle(allocation.texFirst + index + arrayElement));
    }

    void D3D12HeapDescriptorEncoder::WriteSampler(uint32_t index, Sampler *sampler, uint32_t arrayElement)
    {
        auto *smp = static_cast<D3D12Sampler *>(sampler);
        if (smp == nullptr) {
            return;
        }
        native->CreateSampler(&smp->GetNativeDesc(), SamplerHandle(allocation.smpFirst + index + arrayElement));
    }

    // ---- D3D12DescriptorHeap ----

    bool D3D12DescriptorHeap::Init(D3D12Device &inDevice, const Descriptor &desc)
    {
        device         = &inDevice;
        maxTextures    = desc.maxTextures;
        maxBuffers     = desc.maxBuffers;
        const uint32_t resourceCount = desc.maxTextures + desc.maxBuffers;

        auto *native = device->GetNativeHandle();

        if (resourceCount > 0) {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = resourceCount;
            heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            const HRESULT hr = native->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(resourceHeap.GetAddressOf()));
            if (FAILED(hr)) {
                LOG_E(TAG, "descriptor heap (resource) creation failed: 0x%08x", static_cast<unsigned>(hr));
                return false;
            }
            texFree.push_back({0, desc.maxTextures});
            bufFree.push_back({desc.maxTextures, desc.maxBuffers});
        }

        if (desc.maxSamplers > 0) {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            heapDesc.NumDescriptors = desc.maxSamplers;
            heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            const HRESULT hr = native->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(samplerHeap.GetAddressOf()));
            if (FAILED(hr)) {
                LOG_E(TAG, "descriptor heap (sampler) creation failed: 0x%08x", static_cast<unsigned>(hr));
                return false;
            }
            smpFree.push_back({0, desc.maxSamplers});
        }

        resourceIncrement = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        samplerIncrement  = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        return true;
    }

    bool D3D12DescriptorHeap::AllocateFrom(std::vector<FreeRange> &freeList, uint32_t count, uint32_t &outFirst)
    {
        if (count == 0) {
            outFirst = 0;
            return true;
        }
        for (auto it = freeList.begin(); it != freeList.end(); ++it) {
            if (it->count >= count) {
                outFirst = it->first;
                it->first += count;
                it->count -= count;
                if (it->count == 0) {
                    freeList.erase(it);
                }
                return true;
            }
        }
        return false;
    }

    void D3D12DescriptorHeap::FreeTo(std::vector<FreeRange> &freeList, uint32_t first, uint32_t count)
    {
        if (count == 0) {
            return;
        }
        freeList.push_back({first, count});
        std::sort(freeList.begin(), freeList.end(), [](const FreeRange &a, const FreeRange &b) {
            return a.first < b.first;
        });
        std::vector<FreeRange> merged;
        merged.reserve(freeList.size());
        for (const auto &range : freeList) {
            if (!merged.empty() && merged.back().first + merged.back().count == range.first) {
                merged.back().count += range.count;
            } else {
                merged.push_back(range);
            }
        }
        freeList = std::move(merged);
    }

    DescriptorHeap::Allocation D3D12DescriptorHeap::Allocate(const Descriptor &desc)
    {
        Allocation allocation;
        if (!AllocateFrom(texFree, desc.maxTextures, allocation.texFirst)) {
            return {};
        }
        if (!AllocateFrom(bufFree, desc.maxBuffers, allocation.bufFirst)) {
            FreeTo(texFree, allocation.texFirst, desc.maxTextures);
            return {};
        }
        if (!AllocateFrom(smpFree, desc.maxSamplers, allocation.smpFirst)) {
            FreeTo(texFree, allocation.texFirst, desc.maxTextures);
            FreeTo(bufFree, allocation.bufFirst, desc.maxBuffers);
            return {};
        }
        allocation.texCount = desc.maxTextures;
        allocation.bufCount = desc.maxBuffers;
        allocation.smpCount = desc.maxSamplers;
        return allocation;
    }

    void D3D12DescriptorHeap::Free(const Allocation &allocation)
    {
        FreeTo(texFree, allocation.texFirst, allocation.texCount);
        FreeTo(bufFree, allocation.bufFirst, allocation.bufCount);
        FreeTo(smpFree, allocation.smpFirst, allocation.smpCount);
    }

    void D3D12DescriptorHeap::Update(const Allocation & /*allocation*/, DescriptorEncoder & /*encoder*/)
    {
        // D3D12 descriptors are written immediately by the heap encoder.
    }

    std::unique_ptr<DescriptorEncoder> D3D12DescriptorHeap::CreateHeapEncoder(const Allocation &allocation)
    {
        return std::make_unique<D3D12HeapDescriptorEncoder>(*device, resourceHeap.Get(), samplerHeap.Get(),
                                                            resourceIncrement, samplerIncrement, allocation);
    }

} // namespace sky::aurora
