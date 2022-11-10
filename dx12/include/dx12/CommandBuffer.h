//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <rhi/CommandBuffer.h>
#include <dx12/DevObject.h>

namespace sky::dx {
    class Device;

    class CommandBuffer : public rhi::CommandBuffer, public DevObject {
    public:
        CommandBuffer(Device &dev);
        ~CommandBuffer() override;

//        D3D12_RESOURCE_TRANSITION_BARRIER Transition;
//        D3D12_RESOURCE_ALIASING_BARRIER Aliasing;
//        D3D12_RESOURCE_UAV_BARRIER UAV;
        void TransitionBarrier();
        void AliasingBarrier();
        void UAVBarrier();

        void ResourceBarrier();

    private:
        friend class Queue;
        bool Init(const Descriptor &, ComPtr<ID3D12GraphicsCommandList> &cmdList);

        ComPtr<ID3D12GraphicsCommandList> commandList;
    };
    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;
}