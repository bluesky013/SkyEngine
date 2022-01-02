//
// Created by Zach Lee on 2022/1/2.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vk_mem_alloc.h"

namespace sky::drv {

    class Memory : public DevObject {
    public:
        ~Memory() = default;

        struct Descriptor {

        };

    private:
        friend class Device;
        Memory(Device&);
        bool Init(const Descriptor&);
    };


}