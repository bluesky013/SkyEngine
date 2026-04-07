//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/CommandBuffer.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <memory>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12CommandBuffer : public CommandBuffer {
    public:
        D3D12CommandBuffer(D3D12Device &device, ComPtr<ID3D12GraphicsCommandList> cmdList, ComPtr<ID3D12CommandAllocator> allocator);
        ~D3D12CommandBuffer() override;

        void Begin() override;
        void End() override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override;

        ID3D12GraphicsCommandList *GetNativeHandle() const { return cmdList.Get(); }

    private:
        D3D12Device                      &device;
        ComPtr<ID3D12GraphicsCommandList> cmdList;
        ComPtr<ID3D12CommandAllocator>    allocator;
    };

    class D3D12CommandPool : public CommandPool {
    public:
        D3D12CommandPool(D3D12Device &device, D3D12_COMMAND_LIST_TYPE type);
        ~D3D12CommandPool() override;

        bool Init() override;
        void Reset() override;
        CommandBuffer *Allocate() override;

    private:
        D3D12Device             &device;
        D3D12_COMMAND_LIST_TYPE  listType;

        ComPtr<ID3D12CommandAllocator> allocator;
        std::vector<D3D12CommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora

