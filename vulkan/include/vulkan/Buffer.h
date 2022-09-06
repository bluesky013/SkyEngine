//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vk_mem_alloc.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Buffer : public DevObject {
    public:
        ~Buffer();

        struct Descriptor {
            VkDeviceSize        size      = 0;
            VkBufferUsageFlags  usage     = 0;
            VmaMemoryUsage      memory    = VMA_MEMORY_USAGE_UNKNOWN;
            VkBufferCreateFlags flags     = 0;
            bool                transient = false;
        };

        VkBuffer GetNativeHandle() const;

        bool IsTransient() const;

        uint8_t *Map();

        void UnMap();

    private:
        friend class Device;
        Buffer(Device &);

        bool Init(const Descriptor &);

        VkBuffer           buffer;
        VmaAllocation      allocation;
        VkBufferCreateInfo bufferInfo;
        Descriptor         desc        = {};
        bool               isTransient = false;
        uint8_t*           mappedPtr   = nullptr;
    };

    using BufferPtr = std::shared_ptr<Buffer>;

} // namespace sky::drv