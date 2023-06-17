//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Queue.h>
#include <mtl/DevObject.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLIOCommandQueue.h>

namespace sky::mtl {
    class Device;

    class Queue : public rhi::Queue, public DevObject {
    public:
        Queue(Device &dev) : DevObject(dev) {}
        ~Queue();

        rhi::TransferTaskHandle UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests) override;
        rhi::TransferTaskHandle UploadBuffer(const rhi::BufferPtr &image, const std::vector<rhi::BufferUploadRequest> &requests) override;

        id<MTLCommandQueue> GetNativeHandle() const { return queue; }
    private:
        friend class Device;

        id<MTLCommandQueue> queue;
    };
    using QueuePtr = std::unique_ptr<Queue>;

} // namespace sky::mtl
