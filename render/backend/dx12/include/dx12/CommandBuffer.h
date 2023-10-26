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
        explicit CommandBuffer(Device &dev);
        ~CommandBuffer() override = default;

        void Begin() override {}
        void End() override {}
        void Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit) override {}

        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override { return nullptr; }
        std::shared_ptr<rhi::BlitEncoder> EncodeBlit() override { return nullptr; }

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