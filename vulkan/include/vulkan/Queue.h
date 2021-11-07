//
// Created by Zach Lee on 2021/11/7.
//

#pragma once

#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Queue {
    public:
        ~Queue() = default;

        uint32_t GetQueueFamilyIndex() const { return queueFamilyIndex; }

        VkQueue GetNativeHandle() const { return queue; }

    private:
        friend class Device;
        Queue(VkQueue q, uint32_t family, uint32_t index = 0) : queueFamilyIndex(family), queueIndex(index), queue(q) {}

        uint32_t queueFamilyIndex;
        uint32_t queueIndex;
        VkQueue queue;
    };

}
