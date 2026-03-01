//
// Created by Zach Lee on 2022/11/6.
//

#include <dx12/Queue.h>
#include <dx12/Device.h>


namespace sky::dx {
    Queue::Queue(Device &dev) : DevObject(dev)
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

    bool Queue::Init(const Descriptor &desc)
    {
        /**
         * D3D12_COMMAND_LIST_TYPE_DIRECT  0 : direct command buffer
         * D3D12_COMMAND_LIST_TYPE_BUNDLE  1 : secondary command buffer
         * D3D12_COMMAND_LIST_TYPE_COMPUTE 2 : command buffer for computing
         * D3D12_COMMAND_LIST_TYPE_COPY	   3 : command buffer for copying
         */
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        if (desc.type == rhi::QueueType::COMPUTE) {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        } else if (desc.type == rhi::QueueType::TRANSFER) {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        } else {
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        }

        queueDesc.NodeMask = 1;
        if (FAILED(device.GetDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(queue.GetAddressOf())))) {
            return false;
        }

        pool = device.CreateDeviceObject<CommandPool>({});
        return static_cast<bool>(pool);
    }
}