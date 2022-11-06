//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <dx12/DevObject.h>
#include <dx12/CommandBuffer.h>

namespace sky::dx {
    class Device;

    class Queue : public DevObject {
    public:
        Queue(Device &device);
        ~Queue();

        struct Descriptor {
        };

        CommandBufferPtr AllocateCommandBuffer(const CommandBuffer::Descriptor &desc);

        ID3D12CommandQueue *GetNativeQueue() const;
        ID3D12CommandAllocator *GetCommandAllocator() const;

    private:
        friend class Device;
        bool Init(const Descriptor &);

        D3D12_COMMAND_QUEUE_DESC desc;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12CommandAllocator> allocator;
    };
    using QueuePtr = std::unique_ptr<Queue>;
}