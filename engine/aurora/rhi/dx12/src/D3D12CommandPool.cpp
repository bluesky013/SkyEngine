//
// Created by blues on 2026/3/29.
//

#include <D3D12Buffer.h>
#include <D3D12CommandPool.h>
#include <D3D12Conversion.h>
#include <D3D12Device.h>
#include <D3D12Encoder.h>
#include <D3D12Image.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {
    const Device *D3D12CommandBuffer::GetDevice() const noexcept
    {
        return &device;
    }

    // ---- D3D12CommandBuffer ----

    D3D12CommandBuffer::D3D12CommandBuffer(D3D12Device                       &device,
                                           ComPtr<ID3D12GraphicsCommandList>  cmdList,
                                           ComPtr<ID3D12CommandAllocator>     allocator,
                                           ComPtr<ID3D12GraphicsCommandList1> extended)
        : listType(cmdList->GetType())
        , device(device)
        , cmdList(std::move(cmdList))
        , extended(std::move(extended))
        , requiresExtended(this->extended != nullptr)
        , allocator(std::move(allocator))
    {
    }

    D3D12CommandBuffer::~D3D12CommandBuffer()
    {
        cmdList.Reset();
    }

    void D3D12CommandBuffer::Begin()
    {
        if (TryResetAllocator().Succeeded())
            (void)BeginAfterPoolReset();
    }

    void D3D12CommandBuffer::End()
    {
        (void)EndChecked();
    }

    HRESULT D3D12CommandBuffer::BeginAfterPoolReset(bool requireExtended) noexcept
    {
        const auto &fn = device.GetFunctions();
        if (requireExtended && !requiresExtended) {
            cmdList.Reset();
            requiresExtended = true;
        }
        if (!cmdList) {
            extended.Reset();
            auto result = requiresExtended ? fn.createList1(device.GetNativeHandle(), listType, allocator.Get(), extended.GetAddressOf())
                                           : fn.createList(device.GetNativeHandle(), listType, allocator.Get(), cmdList.GetAddressOf());
            if (SUCCEEDED(result) && requiresExtended)
                cmdList = extended;
            if (SUCCEEDED(result))
                result = fn.closeList(cmdList.Get());
            if (FAILED(result)) {
                cmdList.Reset();
                extended.Reset();
                return result;
            }
        }
        const auto result = fn.resetList(cmdList.Get(), allocator.Get());
        if (FAILED(result)) {
            cmdList.Reset();
            extended.Reset();
        }
        return result;
    }
    HRESULT D3D12CommandBuffer::EndChecked() noexcept
    {
        if (!cmdList)
            return E_FAIL;
        const auto result = device.GetFunctions().closeList(cmdList.Get());
        // A list whose Close failed can never be reset; recreate it after this lease drains.
        if (FAILED(result)) {
            cmdList.Reset();
            extended.Reset();
        }
        return result;
    }

    void D3D12CommandBuffer::ResetAllocator()
    {
        (void)TryResetAllocator();
    }

    GpuOperationResult D3D12CommandBuffer::TryResetAllocator() noexcept
    {
        if (!allocator)
            return {{GpuErrorCode::INVALID_ARGUMENT}};
        const auto result = device.GetFunctions().resetAllocator(allocator.Get());
        if (SUCCEEDED(result))
            return {};
        return {D3D12Error(result)};
    }

    void D3D12CommandBuffer::PipelineBarrier(const BarrierInfo &info)
    {
        std::vector<D3D12_RESOURCE_BARRIER> barriers;
        barriers.reserve(info.imageBarriers.size() + info.bufferBarriers.size() + info.memoryBarriers.size());

        for (const auto &ib : info.imageBarriers) {
            D3D12_RESOURCE_BARRIER b = {};
            b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            b.Transition.pResource   = static_cast<D3D12Image *>(ib.image)->GetNativeHandle();
            b.Transition.StateBefore = ToD3D12States(ib.srcAccess);
            b.Transition.StateAfter  = ToD3D12States(ib.dstAccess);
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers.push_back(b);
        }

        for (const auto &bb : info.bufferBarriers) {
            D3D12_RESOURCE_BARRIER      b      = {};
            const D3D12_RESOURCE_STATES before = ToD3D12States(bb.srcAccess);
            const D3D12_RESOURCE_STATES after  = ToD3D12States(bb.dstAccess);
            if (before == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && after == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
                b.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                b.UAV.pResource = static_cast<D3D12Buffer *>(bb.buffer)->GetNativeHandle();
            } else {
                b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.pResource   = static_cast<D3D12Buffer *>(bb.buffer)->GetNativeHandle();
                b.Transition.StateBefore = before;
                b.Transition.StateAfter  = after;
                b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            barriers.push_back(b);
        }

        for (const auto &m : info.memoryBarriers) {
            (void)m;
            D3D12_RESOURCE_BARRIER b = {};
            b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            b.UAV.pResource          = nullptr; // global UAV barrier
            barriers.push_back(b);
        }

        if (!barriers.empty()) {
            cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }
    }

    std::unique_ptr<GraphicsEncoder> D3D12CommandBuffer::CreateGraphicsEncoder()
    {
        return std::make_unique<D3D12GraphicsEncoder>(device, cmdList.Get());
    }

    std::unique_ptr<ComputeEncoder> D3D12CommandBuffer::CreateComputeEncoder()
    {
        return std::make_unique<D3D12ComputeEncoder>(device, cmdList.Get());
    }

    std::unique_ptr<BlitEncoder> D3D12CommandBuffer::CreateBlitEncoder()
    {
        return std::make_unique<D3D12BlitEncoder>(device, cmdList.Get());
    }

    // ---- D3D12CommandPool ----

    D3D12CommandPool::D3D12CommandPool(D3D12Device &device, D3D12_COMMAND_LIST_TYPE type) : device(device), listType(type)
    {
    }

    D3D12CommandPool::~D3D12CommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();
    }

    bool D3D12CommandPool::Init()
    {
        // Each command buffer owns its allocator (created in Allocate).
        return true;
    }

    void D3D12CommandPool::Reset()
    {
        (void)TryReset();
    }

    GpuOperationResult D3D12CommandPool::TryReset() noexcept
    {
        for (auto *buffer : allocatedBuffers) {
            const auto result = buffer->TryResetAllocator();
            if (!result.Succeeded())
                return result;
        }
        return {};
    }

    CommandBuffer *D3D12CommandPool::Allocate()
    {
        auto result = AllocateChecked();
        return std::holds_alternative<D3D12CommandBuffer *>(result) ? std::get<D3D12CommandBuffer *>(result) : nullptr;
    }
    std::variant<GpuError, D3D12CommandBuffer *> D3D12CommandPool::AllocateChecked(bool requireExtended)
    {
        const auto                    &fn = device.GetFunctions();
        ComPtr<ID3D12CommandAllocator> allocator;
        auto                           result = fn.createAllocator(device.GetNativeHandle(), listType, allocator.GetAddressOf());
        if (FAILED(result))
            return D3D12Error(result);
        ComPtr<ID3D12GraphicsCommandList>  list;
        ComPtr<ID3D12GraphicsCommandList1> extended;
        result = requireExtended ? fn.createList1(device.GetNativeHandle(), listType, allocator.Get(), extended.GetAddressOf())
                                 : fn.createList(device.GetNativeHandle(), listType, allocator.Get(), list.GetAddressOf());
        if (FAILED(result))
            return D3D12Error(result);
        if (requireExtended)
            list = extended;
        result = fn.closeList(list.Get());
        if (FAILED(result))
            return D3D12Error(result);
        auto *command = new D3D12CommandBuffer(device, std::move(list), std::move(allocator), std::move(extended));
        allocatedBuffers.push_back(command);
        return command;
    }
} // namespace sky::aurora
