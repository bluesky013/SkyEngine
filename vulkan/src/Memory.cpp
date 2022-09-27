//
// Created by Zach Lee on 2022/1/2.
//

#include <vulkan/Memory.h>
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace sky::drv {

    Memory::Memory(Device &dev) : DevObject(dev)
    {
    }

    bool Memory::Init(const Descriptor &)
    {
        return true;
    }
} // namespace sky::drv
