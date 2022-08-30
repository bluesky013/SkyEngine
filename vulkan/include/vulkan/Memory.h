//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include "vk_mem_alloc.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Memory : public DevObject {
    public:
        ~Memory() = default;

        struct Descriptor {};

    private:
        friend class Device;
        Memory(Device &);
        bool Init(const Descriptor &);
    };

} // namespace sky::drv