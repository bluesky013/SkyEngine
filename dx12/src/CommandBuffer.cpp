//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/CommandBuffer.h>
#include <dx12/Device.h>

namespace sky::dx {

    CommandBuffer::CommandBuffer(Device &dev) : DevObject(dev)
    {
    }

    CommandBuffer::~CommandBuffer()
    {
    }

    void CommandBuffer::TransitionBarrier()
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = nullptr;
        barrier.Transition.StateBefore;
        barrier.Transition.StateAfter;
        commandList->ResourceBarrier(1, &barrier);
    }

    void CommandBuffer::AliasingBarrier()
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        barrier.Aliasing.pResourceBefore = nullptr;
        barrier.Aliasing.pResourceAfter = nullptr;
        commandList->ResourceBarrier(1, &barrier);
    }

    void CommandBuffer::UAVBarrier()
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = nullptr;
        commandList->ResourceBarrier(1, &barrier);
    }

    bool CommandBuffer::Init(const Descriptor &desc, ComPtr<ID3D12GraphicsCommandList> &cmdList)
    {
        commandList = cmdList;
        return true;
    }
}