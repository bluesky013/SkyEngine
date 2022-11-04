//
// Created by Zach Lee on 2022/11/4.
//

#include <vulkan/TransientPool.h>
#include <vulkan/Device.h>

namespace sky::vk {

    int32_t TransientPool::GetMemoryTypeIndex(Device &device)
    {
        VkMemoryRequirements bufferReq = {};
        VkMemoryRequirements imageReq = {};
        {
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = 4;
            bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            VkBuffer buffer = VK_NULL_HANDLE;
            vkCreateBuffer(device.GetNativeHandle(), &bufferInfo, nullptr, &buffer);
            vkGetBufferMemoryRequirements(device.GetNativeHandle(), buffer, &bufferReq);
            vkDestroyBuffer(device.GetNativeHandle(), buffer, nullptr);
        }

        {
            VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.extent = {1, 1, 1};
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_STORAGE_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImage image = VK_NULL_HANDLE;
            vkCreateImage(device.GetNativeHandle(), &imageInfo, nullptr, &image);
            vkGetImageMemoryRequirements(device.GetNativeHandle(), image, &imageReq);
            vkDestroyImage(device.GetNativeHandle(), image, nullptr);
        }

        return device.FindProperties(bufferReq.memoryTypeBits & imageReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    }

    bool TransientPool::Init(const Descriptor &desc)
    {
        int32_t memIndex = GetMemoryTypeIndex(device);
        if (memIndex < 0) {
            return false;
        }

        auto allocator = device.GetAllocator();
        VmaPoolCreateInfo poolInfo = {};
        poolInfo.blockSize = desc.blockSize;
        poolInfo.memoryTypeIndex = static_cast<uint32_t>(memIndex);
        vmaCreatePool(allocator, &poolInfo, &pool);
        return pool != VK_NULL_HANDLE;
    }

    TransientPool::TransientPool(Device &dev) : device(dev), pool(VK_NULL_HANDLE)
    {
    }

    void TransientPool::BeginFrame()
    {
    }

    void TransientPool::EndFrame()
    {
    }

    void TransientPool::InitBuffer(const BufferPtr &buffer)
    {
        SKY_ASSERT(buffer->IsTransient());
        VmaAllocationCreateInfo bufferMemInfo = {};
        bufferMemInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferMemInfo.pool                    = pool;

        VmaAllocation     allocation     = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};
        vmaAllocateMemoryForBuffer(device.GetAllocator(),
                                   buffer->GetNativeHandle(),
                                   &bufferMemInfo,
                                   &allocation,
                                   &allocationInfo);
    }

    void TransientPool::ResetBuffer(const BufferPtr &buffer)
    {
        SKY_ASSERT(buffer->IsTransient());
        buffer->ReleaseMemory();
    }

    void TransientPool::InitImage(const ImagePtr &image)
    {
        SKY_ASSERT(image->IsTransient());
        VmaAllocationCreateInfo bufferMemInfo = {};
        bufferMemInfo.usage                   = VMA_MEMORY_USAGE_GPU_ONLY;
        bufferMemInfo.pool                    = pool;

        VmaAllocation     allocation     = VK_NULL_HANDLE;
        VmaAllocationInfo allocationInfo = {};
        vmaAllocateMemoryForImage(device.GetAllocator(),
                                   image->GetNativeHandle(),
                                   &bufferMemInfo,
                                   &allocation,
                                   &allocationInfo);
    }

    void TransientPool::ResetImage(const ImagePtr &image)
    {
        SKY_ASSERT(image->IsTransient());
        image->ReleaseMemory();
    }
}
