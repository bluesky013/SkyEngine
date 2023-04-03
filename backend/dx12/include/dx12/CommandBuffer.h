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

        void Begin() override {}
        void End() override {}
        void Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit) override {}

        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override { return nullptr; }

        void TransitionBarrier();
        void AliasingBarrier();
        void UAVBarrier();

        void ResourceBarrier();

    private:
        friend class Queue;
        friend class CommandPool;
        bool Init(const Descriptor &, ComPtr<ID3D12CommandAllocator> &allocator, D3D12_COMMAND_LIST_TYPE type);

        D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
    };
    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;
}