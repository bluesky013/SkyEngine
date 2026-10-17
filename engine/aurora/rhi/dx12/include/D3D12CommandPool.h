//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/CommandBuffer.h>
#include <d3d12.h>
#include <memory>
#include <variant>
#include <vector>
#include <wrl/client.h>

namespace sky::aurora {

    using Microsoft::WRL::ComPtr;

    class D3D12Device;

    class D3D12CommandBuffer : public CommandBuffer {
    public:
        const Device *GetDevice() const noexcept override;
        D3D12CommandBuffer(D3D12Device                       &device,
                           ComPtr<ID3D12GraphicsCommandList>  cmdList,
                           ComPtr<ID3D12CommandAllocator>     allocator,
                           ComPtr<ID3D12GraphicsCommandList1> extended = {});
        ~D3D12CommandBuffer() override;

        void Begin() override;
        void End() override;
        void PipelineBarrier(const BarrierInfo &info) override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder>  CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder>     CreateBlitEncoder() override;

        ID3D12GraphicsCommandList *GetNativeHandle() const
        {
            return cmdList.Get();
        }
        ID3D12GraphicsCommandList1 *GetNativeHandle1() const
        {
            return extended.Get();
        }

        // Each command buffer owns its allocator; reset only this buffer's.
        void               ResetAllocator();
        GpuOperationResult TryResetAllocator() noexcept;
        HRESULT            BeginAfterPoolReset(bool requireExtended = false) noexcept;
        HRESULT            EndChecked() noexcept;

    private:
        D3D12_COMMAND_LIST_TYPE            listType;
        D3D12Device                       &device;
        ComPtr<ID3D12GraphicsCommandList>  cmdList;
        ComPtr<ID3D12GraphicsCommandList1> extended;
        bool                               requiresExtended = false;
        ComPtr<ID3D12CommandAllocator>     allocator;
    };

    class D3D12CommandPool : public CommandPool {
    public:
        D3D12CommandPool(D3D12Device &device, D3D12_COMMAND_LIST_TYPE type);
        ~D3D12CommandPool() override;

        bool                                         Init() override;
        void                                         Reset() override;
        GpuOperationResult                           TryReset() noexcept override;
        CommandBuffer                               *Allocate() override;
        std::variant<GpuError, D3D12CommandBuffer *> AllocateChecked(bool requireExtended = false);

    private:
        D3D12Device            &device;
        D3D12_COMMAND_LIST_TYPE listType;

        std::vector<D3D12CommandBuffer *> allocatedBuffers;
    };

} // namespace sky::aurora
