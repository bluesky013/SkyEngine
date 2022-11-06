//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/Queue.h>
#include <dx12/Device.h>


namespace sky::dx {
    Queue::Queue(Device &dev) : DevObject(dev)
    {
    }

    Queue::~Queue()
    {
    }

    CommandBufferPtr Queue::AllocateCommandBuffer(const CommandBuffer::Descriptor &cmdDesc)
    {
        ComPtr<ID3D12GraphicsCommandList> commandList;
        if (SUCCEEDED(device.GetDevice()->CreateCommandList(0, desc.Type, allocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())))) {
            auto cmdBuffer = std::make_shared<CommandBuffer>(device);
            cmdBuffer->Init(cmdDesc, commandList);
            return cmdBuffer;
        }
        return {};
    }

    ID3D12CommandQueue *Queue::GetNativeQueue() const
    {
        return queue.Get();
    }

    ID3D12CommandAllocator *Queue::GetCommandAllocator() const
    {
        return allocator.Get();
    }

    bool Queue::Init(const Descriptor &)
    {
        /**
         * D3D12_COMMAND_LIST_TYPE_DIRECT  0 : direct command buffer
         * D3D12_COMMAND_LIST_TYPE_BUNDLE  1 : secondary command buffer
         * D3D12_COMMAND_LIST_TYPE_COMPUTE 2 : command buffer for computing
         * D3D12_COMMAND_LIST_TYPE_COPY	   3 : command buffer for copying
         */
        desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        if (FAILED(device.GetDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(queue.GetAddressOf())))) {
            return false;
        }

        if (FAILED(device.GetDevice()->CreateCommandAllocator(desc.Type, IID_PPV_ARGS(allocator.GetAddressOf())))) {
            return false;
        }

        return true;
    }
}