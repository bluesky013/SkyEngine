//
// Created by Zach Lee on 2022/10/22.
//

#include <vulkan/StreamingPool.h>
#include <vulkan/Device.h>

namespace sky::drv {

    void StreamingPool::Setup(const Descriptor &desc)
    {
        info = desc;

        drv::Buffer::Descriptor bufferDesc = {};
        bufferDesc.size   = info.blockSize;
        bufferDesc.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferDesc.memory = VMA_MEMORY_USAGE_CPU_ONLY;

        stagingBufferPool = device.CreateDeviceObject<Buffer>(bufferDesc);
    }

    void StreamingPool::UploadBuffer(const BufferPtr &buffer, const BufferPackage &package)
    {
        auto *asyncQueue = device.GetAsyncTransferQueue();
    }

    void StreamingPool::UploadImage(const ImagePtr &image, const ImagePackage &package)
    {
        auto *asyncQueue = device.GetAsyncTransferQueue();
    }
}