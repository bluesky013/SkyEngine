//
// Created by blues on 2026/3/29.
//

#include <D3D12CommandPool.h>
#include <D3D12Device.h>
#include <D3D12Encoder.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraDX12";

namespace sky::aurora {

    // ---- D3D12CommandBuffer ----

    D3D12CommandBuffer::D3D12CommandBuffer(D3D12Device &device, ComPtr<ID3D12GraphicsCommandList> cmdList, ComPtr<ID3D12CommandAllocator> allocator)
        : device(device)
        , cmdList(std::move(cmdList))
        , allocator(std::move(allocator))
    {
    }

    D3D12CommandBuffer::~D3D12CommandBuffer()
    {
        cmdList.Reset();
    }

    void D3D12CommandBuffer::Begin()
    {
        allocator->Reset();
        cmdList->Reset(allocator.Get(), nullptr);
    }

    void D3D12CommandBuffer::End()
    {
        cmdList->Close();
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

    D3D12CommandPool::D3D12CommandPool(D3D12Device &device, D3D12_COMMAND_LIST_TYPE type)
        : device(device)
        , listType(type)
    {
    }

    D3D12CommandPool::~D3D12CommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();
        allocator.Reset();
    }

    bool D3D12CommandPool::Init()
    {
        HRESULT hr = device.GetNativeHandle()->CreateCommandAllocator(listType, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create ID3D12CommandAllocator, HRESULT: 0x%08x", hr);
            return false;
        }
        return true;
    }

    void D3D12CommandPool::Reset()
    {
        allocator->Reset();
    }

    CommandBuffer *D3D12CommandPool::Allocate()
    {
        ComPtr<ID3D12GraphicsCommandList> cmdList;
        HRESULT hr = device.GetNativeHandle()->CreateCommandList(
            0, listType, allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));
        if (FAILED(hr)) {
            LOG_E(TAG, "failed to create ID3D12GraphicsCommandList, HRESULT: 0x%08x", hr);
            return nullptr;
        }

        // command list is created in recording state, close it so user calls Begin() to start
        cmdList->Close();

        auto *cmdBuffer = new D3D12CommandBuffer(device, std::move(cmdList), allocator);
        allocatedBuffers.emplace_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora

