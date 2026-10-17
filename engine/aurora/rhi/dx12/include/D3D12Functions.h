//
// Created on 2026/10/15.
//

#pragma once

#include <aurora/rhi/SubmissionResult.h>
#include <d3d12.h>
#include <dxgi.h>

namespace sky::aurora {
    inline GpuError D3D12Error(HRESULT result) noexcept
    {
        const auto code = result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET || result == DXGI_ERROR_DEVICE_HUNG
                              ? GpuErrorCode::DEVICE_LOST
                          : result == E_OUTOFMEMORY ? GpuErrorCode::OUT_OF_MEMORY
                                                    : GpuErrorCode::NATIVE_ERROR;
        return {code, static_cast<int64_t>(result)};
    }

    // Native call boundary; installed functions remain immutable while device work is running.
    struct D3D12Functions {
        HRESULT (*present)(IDXGISwapChain *, UINT, UINT) = [](IDXGISwapChain *s, UINT interval, UINT flags) { return s->Present(interval, flags); };
        HRESULT (*resizeBuffers)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT) =
            [](IDXGISwapChain *s, UINT count, UINT width, UINT height, DXGI_FORMAT format, UINT flags) {
                return s->ResizeBuffers(count, width, height, format, flags);
            };
        HRESULT (*options2)(ID3D12Device *, D3D12_FEATURE_DATA_D3D12_OPTIONS2 *) = [](ID3D12Device *d, D3D12_FEATURE_DATA_D3D12_OPTIONS2 *info) {
            return d->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, info, sizeof(*info));
        };
        HRESULT (*createList1)(ID3D12Device *, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *, ID3D12GraphicsCommandList1 **) =
            [](ID3D12Device *d, D3D12_COMMAND_LIST_TYPE t, ID3D12CommandAllocator *a, ID3D12GraphicsCommandList1 **out) {
                return d->CreateCommandList(0, t, a, nullptr, IID_PPV_ARGS(out));
            };
        HRESULT (*multisampleQuality)(ID3D12Device *, D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS *) =
            [](ID3D12Device *d, D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS *info) {
                return d->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, info, sizeof(*info));
            };
        HRESULT (*createAllocator)(ID3D12Device *, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator **) =
            [](ID3D12Device *d, D3D12_COMMAND_LIST_TYPE t, ID3D12CommandAllocator **out) { return d->CreateCommandAllocator(t, IID_PPV_ARGS(out)); };
        HRESULT (*createList)(ID3D12Device *, D3D12_COMMAND_LIST_TYPE, ID3D12CommandAllocator *, ID3D12GraphicsCommandList **) =
            [](ID3D12Device *d, D3D12_COMMAND_LIST_TYPE t, ID3D12CommandAllocator *a, ID3D12GraphicsCommandList **out) {
                return d->CreateCommandList(0, t, a, nullptr, IID_PPV_ARGS(out));
            };
        HRESULT (*resetAllocator)(ID3D12CommandAllocator *)                         = [](ID3D12CommandAllocator *a) { return a->Reset(); };
        HRESULT (*resetList)(ID3D12GraphicsCommandList *, ID3D12CommandAllocator *) = [](ID3D12GraphicsCommandList *c, ID3D12CommandAllocator *a) {
            return c->Reset(a, nullptr);
        };
        HRESULT (*closeList)(ID3D12GraphicsCommandList *) = [](ID3D12GraphicsCommandList *c) { return c->Close(); };
        void (*draw)(ID3D12GraphicsCommandList *, UINT, UINT, UINT, UINT) =
            [](ID3D12GraphicsCommandList *c, UINT vertices, UINT instances, UINT firstVertex, UINT firstInstance) {
                c->DrawInstanced(vertices, instances, firstVertex, firstInstance);
            };
        HRESULT (*createGraphicsPipeline)(ID3D12Device *, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *, ID3D12PipelineState **) =
            [](ID3D12Device *d, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *desc, ID3D12PipelineState **out) {
                return d->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(out));
            };
        HRESULT (*createDescriptorHeap)(ID3D12Device *, const D3D12_DESCRIPTOR_HEAP_DESC *, ID3D12DescriptorHeap **) =
            [](ID3D12Device *d, const D3D12_DESCRIPTOR_HEAP_DESC *desc, ID3D12DescriptorHeap **out) {
                return d->CreateDescriptorHeap(desc, IID_PPV_ARGS(out));
            };
        void (*execute)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *) =
            [](ID3D12CommandQueue *q, UINT count, ID3D12CommandList *const *lists) { q->ExecuteCommandLists(count, lists); };
        HRESULT (*signal)(ID3D12CommandQueue *, ID3D12Fence *, UINT64) = [](ID3D12CommandQueue *q, ID3D12Fence *f, UINT64 value) {
            return q->Signal(f, value);
        };
        HRESULT (*wait)(ID3D12CommandQueue *, ID3D12Fence *, UINT64) = [](ID3D12CommandQueue *q, ID3D12Fence *f, UINT64 value) {
            return q->Wait(f, value);
        };
        UINT64 (*completed)(ID3D12Fence *) = [](ID3D12Fence *f) { return f->GetCompletedValue(); };
        HRESULT (*removed)(ID3D12Device *) = [](ID3D12Device *d) { return d->GetDeviceRemovedReason(); };
    };
} // namespace sky::aurora
