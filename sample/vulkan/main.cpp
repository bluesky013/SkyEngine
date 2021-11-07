//
// Created by Zach Lee on 2021/11/7.
//
#include "vulkan/Driver.h"
#include "vulkan/Buffer.h"
#include "vulkan/Image.h"
#include "vulkan/CommandPool.h"
#include "vulkan/CommandBuffer.h"

int main()
{
    using namespace sky::drv;

    auto driver = Driver::Create({"", "", true});

    auto device = driver->CreateDevice({});

    Buffer::Descriptor bufferDes = {};
    bufferDes.size   = 128;
    bufferDes.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
    bufferDes.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    auto stagingBuffer = device->CreateDeviceObject<Buffer>(bufferDes);

    bufferDes.size   = 128;
    bufferDes.memory = VMA_MEMORY_USAGE_GPU_ONLY;
    bufferDes.usage  = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    auto deviceBuffer = device->CreateDeviceObject<Buffer>(bufferDes);

    Image::Descriptor imageDes = {};
    imageDes.imageType   = VK_IMAGE_TYPE_2D;
    imageDes.format      = VK_FORMAT_R8G8B8A8_UNORM;
    imageDes.extent      = {4, 4, 1};
    imageDes.mipLevels   = 1;
    imageDes.arrayLayers = 1;
    imageDes.usage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageDes.samples     = VK_SAMPLE_COUNT_1_BIT;
    imageDes.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imageDes.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
    auto image = device->CreateDeviceObject<Image>(imageDes);

    auto queue = device->GetQueue({VK_QUEUE_TRANSFER_BIT});

    CommandPool::Descriptor cmdPoolDes = {};
    cmdPoolDes.queueFamilyIndex = queue->GetQueueFamilyIndex();
    cmdPoolDes.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    auto pool = device->CreateDeviceObject<CommandPool>(cmdPoolDes);

    CommandBuffer::Descriptor cmdDes = {};
    cmdDes.needFence = true;
    auto cmd = pool->Allocate(cmdDes);

    cmd->Begin();

    cmd->Encode([stagingBuffer, deviceBuffer](VkCommandBuffer cmdBuffer) {

        VkBufferCopy copyRegion = {};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = 128;
        vkCmdCopyBuffer(cmdBuffer, stagingBuffer->GetNativeHandle(), deviceBuffer->GetNativeHandle(), 1, &copyRegion);
    });

    cmd->End();
    cmd->Submit(*queue, {});


    delete cmd;
    delete pool;
    delete stagingBuffer;
    delete deviceBuffer;
    delete image;
    delete device;
    Driver::Destroy(driver);
    return 0;
}