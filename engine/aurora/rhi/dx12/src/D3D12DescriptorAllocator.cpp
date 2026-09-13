//
// Created on 2026/09/13.
//

#include <D3D12DescriptorAllocator.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

#include <algorithm>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    bool D3D12DescriptorAllocator::Init(D3D12Device &device, uint32_t cbvSrvUavSize, uint32_t samplerSize)
    {
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
        cbvSrvUavDesc.Type                      = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvSrvUavDesc.NumDescriptors            = cbvSrvUavSize;
        cbvSrvUavDesc.Flags                     = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        HRESULT hr = device.GetNativeHandle()->CreateDescriptorHeap(&cbvSrvUavDesc, IID_PPV_ARGS(cbvSrvUavHeap.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "create CBV/SRV/UAV descriptor heap failed: 0x%08x", static_cast<unsigned>(hr));
            return false;
        }

        D3D12_DESCRIPTOR_HEAP_DESC samplerDesc = {};
        samplerDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        samplerDesc.NumDescriptors             = samplerSize;
        samplerDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        hr = device.GetNativeHandle()->CreateDescriptorHeap(&samplerDesc, IID_PPV_ARGS(samplerHeap.GetAddressOf()));
        if (FAILED(hr)) {
            LOG_E(TAG, "create sampler descriptor heap failed: 0x%08x", static_cast<unsigned>(hr));
            return false;
        }

        cbvSrvUavIncrement = device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        samplerIncrement   = device.GetNativeHandle()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        cbvSrvUavFree.push_back({0, cbvSrvUavSize});
        samplerFree.push_back({0, samplerSize});
        return true;
    }

    bool D3D12DescriptorAllocator::Allocate(uint32_t cbvSrvUavCount, uint32_t samplerCount, DescriptorAllocation &out)
    {
        uint32_t cbvSrvUavFirst = 0;
        uint32_t samplerFirst   = 0;
        if (!AllocateFrom(cbvSrvUavFree, cbvSrvUavCount, cbvSrvUavFirst)) {
            return false;
        }
        if (!AllocateFrom(samplerFree, samplerCount, samplerFirst)) {
            FreeTo(cbvSrvUavFree, cbvSrvUavFirst, cbvSrvUavCount);
            return false;
        }
        out.cbvSrvUavFirst = cbvSrvUavFirst;
        out.cbvSrvUavCount = cbvSrvUavCount;
        out.samplerFirst   = samplerFirst;
        out.samplerCount   = samplerCount;
        return true;
    }

    void D3D12DescriptorAllocator::Free(const DescriptorAllocation &alloc)
    {
        FreeTo(cbvSrvUavFree, alloc.cbvSrvUavFirst, alloc.cbvSrvUavCount);
        FreeTo(samplerFree, alloc.samplerFirst, alloc.samplerCount);
    }

    bool D3D12DescriptorAllocator::AllocateFrom(std::vector<FreeRange> &freeList, uint32_t count, uint32_t &outFirst)
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

    void D3D12DescriptorAllocator::FreeTo(std::vector<FreeRange> &freeList, uint32_t first, uint32_t count)
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

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetCbvSrvUavCpuHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * cbvSrvUavIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetCbvSrvUavGpuHandle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * cbvSrvUavIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetSamplerCpuHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * samplerIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetSamplerGpuHandle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * samplerIncrement;
        return handle;
    }

} // namespace sky::aurora
