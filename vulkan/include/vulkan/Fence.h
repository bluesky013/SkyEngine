//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Fence : public DevObject {
    public:
        ~Fence();

        struct Descriptor {
            VkFenceCreateFlags flag = 0;
        };

        bool Init(const Descriptor&);

        void Wait(uint64_t timeout = UINT64_MAX);

        void Reset();

        VkFence GetNativeHandle() const;

    private:
        friend class Device;
        Fence(Device&);
        VkFence fence;
    };

    using FencePtr = std::shared_ptr<Fence>;

}