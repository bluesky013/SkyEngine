//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include "vk_mem_alloc.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class MemoryPool : public DevObject {
    public:
        ~MemoryPool() = default;

        struct Descriptor {};

    private:
        friend class Device;
        MemoryPool(Device &);
        bool Init(const Descriptor &);

        VmaPool pool;
    };

} // namespace sky::drv