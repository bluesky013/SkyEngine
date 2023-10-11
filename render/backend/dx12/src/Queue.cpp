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
        return pool->Allocate(cmdDesc);
    }

    ID3D12CommandQueue *Queue::GetNativeQueue() const
    {
        return queue.Get();
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

        pool = device.CreateDeviceObject<CommandPool>({});
        return static_cast<bool>(pool);
    }
}