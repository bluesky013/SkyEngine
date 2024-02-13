//
// Created by Zach Lee on 2022/11/6.
//

#pragma once

#include <rhi/Queue.h>
#include <dx12/DevObject.h>
#include <dx12/CommandPool.h>

namespace sky::dx {
    class Device;

    class Queue : public rhi::Queue, public DevObject {
    public:
        explicit Queue(Device &device);
        ~Queue() override = default;

        struct Descriptor {
            rhi::QueueType type;
        };

        CommandBufferPtr AllocateCommandBuffer(const CommandBuffer::Descriptor &desc);

        ID3D12CommandQueue *GetNativeQueue() const;

        rhi::TransferTaskHandle UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests) override { return 0; }
        rhi::TransferTaskHandle UploadBuffer(const rhi::BufferPtr &image, const std::vector<rhi::BufferUploadRequest> &requests) override { return 0; }

    private:
        friend class Device;
        bool Init(const Descriptor &);

        ComPtr<ID3D12CommandQueue> queue;
        CommandPoolPtr pool;
    };
    using QueuePtr = std::unique_ptr<Queue>;
}