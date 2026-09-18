//
// Created on 2026/09/13.
//

#include <D3D12DescriptorAllocator.h>
#include <D3D12Device.h>
#include <core/logger/Logger.h>

#include <algorithm>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    namespace {
        ComPtr<ID3D12DescriptorHeap> CreateHeap(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                                                uint32_t size, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Type           = type;
            desc.NumDescriptors = size;
            desc.Flags          = flags;

            ComPtr<ID3D12DescriptorHeap> heap;
            const HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf()));
            if (FAILED(hr)) {
                LOG_E(TAG, "create descriptor heap (type=%d) failed: 0x%08x", static_cast<int>(type), static_cast<unsigned>(hr));
                return nullptr;
            }
            return heap;
        }
    } // namespace

    bool D3D12DescriptorAllocator::Init(D3D12Device &device, uint32_t cbvSrvUavSize, uint32_t samplerSize, uint32_t ringSize,
                                        uint32_t rtvSizeIn, uint32_t dsvSizeIn)
    {
        mDevice       = &device;
        mRingSize     = ringSize == 0 ? 1 : ringSize;
        this->rtvSize = rtvSizeIn;
        this->dsvSize = dsvSizeIn;

        auto *native = device.GetNativeHandle();

        // CPU-only staging heaps (source of truth)
        cbvSrvUavHeap = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cbvSrvUavSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
        samplerHeap   = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, samplerSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
        if (cbvSrvUavHeap == nullptr || samplerHeap == nullptr) {
            return false;
        }

        // shader-visible ring (one per in-flight frame)
        cbvSrvUavRing.resize(mRingSize);
        samplerRing.resize(mRingSize);
        rtvRing.resize(mRingSize);
        dsvRing.resize(mRingSize);
        for (uint32_t i = 0; i < mRingSize; ++i) {
            cbvSrvUavRing[i] = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, cbvSrvUavSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
            samplerRing[i]   = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, samplerSize, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
            rtvRing[i]       = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, rtvSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
            dsvRing[i]       = CreateHeap(native, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, dsvSize, D3D12_DESCRIPTOR_HEAP_FLAG_NONE);
            if (cbvSrvUavRing[i] == nullptr || samplerRing[i] == nullptr ||
                rtvRing[i] == nullptr || dsvRing[i] == nullptr) {
                return false;
            }
        }

        cbvSrvUavIncrement = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        samplerIncrement   = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
        rtvIncrement       = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsvIncrement       = native->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        cbvSrvUavFree.push_back({0, cbvSrvUavSize});
        samplerFree.push_back({0, samplerSize});
        return true;
    }

    void D3D12DescriptorAllocator::BeginFrame(uint32_t frameIndex)
    {
        mCurrentFrame = frameIndex % mRingSize;
        rtvUsed       = 0;
        dsvUsed       = 0;
    }

    bool D3D12DescriptorAllocator::AllocateRtv(uint32_t count, uint32_t &outFirst)
    {
        if (rtvUsed + count > rtvSize) {
            return false;
        }
        outFirst = rtvUsed;
        rtvUsed += count;
        return true;
    }

    bool D3D12DescriptorAllocator::AllocateDsv(uint32_t &outFirst)
    {
        if (dsvUsed + 1 > dsvSize) {
            return false;
        }
        outFirst = dsvUsed;
        ++dsvUsed;
        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetRtvCpuHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvRing[mCurrentFrame]->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * rtvIncrement;
        return handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetDsvCpuHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvRing[mCurrentFrame]->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * dsvIncrement;
        return handle;
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

    D3D12_CPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetSamplerCpuHandle(uint32_t index) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * samplerIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetCbvSrvUavGpuHandle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = cbvSrvUavRing[mCurrentFrame]->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * cbvSrvUavIncrement;
        return handle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE D3D12DescriptorAllocator::GetSamplerGpuHandle(uint32_t index) const
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle = samplerRing[mCurrentFrame]->GetGPUDescriptorHandleForHeapStart();
        handle.ptr += static_cast<size_t>(index) * samplerIncrement;
        return handle;
    }

    void D3D12DescriptorAllocator::CopyCbvSrvUav(uint32_t first, uint32_t count)
    {
        if (count == 0) {
            return;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dst = cbvSrvUavRing[mCurrentFrame]->GetCPUDescriptorHandleForHeapStart();
        dst.ptr += static_cast<size_t>(first) * cbvSrvUavIncrement;
        mDevice->GetNativeHandle()->CopyDescriptorsSimple(count, dst, GetCbvSrvUavCpuHandle(first),
                                                          D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    void D3D12DescriptorAllocator::CopySampler(uint32_t first, uint32_t count)
    {
        if (count == 0) {
            return;
        }
        D3D12_CPU_DESCRIPTOR_HANDLE dst = samplerRing[mCurrentFrame]->GetCPUDescriptorHandleForHeapStart();
        dst.ptr += static_cast<size_t>(first) * samplerIncrement;
        mDevice->GetNativeHandle()->CopyDescriptorsSimple(count, dst, GetSamplerCpuHandle(first),
                                                          D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    ID3D12DescriptorHeap *D3D12DescriptorAllocator::GetCbvSrvUavHeap() const
    {
        return cbvSrvUavRing[mCurrentFrame].Get();
    }

    ID3D12DescriptorHeap *D3D12DescriptorAllocator::GetSamplerHeap() const
    {
        return samplerRing[mCurrentFrame].Get();
    }

} // namespace sky::aurora
