//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

namespace sky::drv {

    class Device;

    class Buffer : public DevObject {
    public:
        ~Buffer();

        struct Descriptor {
            VkBufferCreateFlags flags  = 0;
            VkDeviceSize        size   = 0;
            VkBufferUsageFlags  usage  = 0;
            VmaMemoryUsage      memory = VMA_MEMORY_USAGE_UNKNOWN;
        };

        bool Init(const Descriptor&);

    private:
        friend class Device;
        Buffer(Device&);

        VkBuffer buffer;
        VmaAllocation allocation;
    };

}