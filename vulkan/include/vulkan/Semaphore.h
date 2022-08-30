//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::drv {

    class Device;

    class Semaphore : public DevObject {
    public:
        ~Semaphore();

        struct Descriptor {};

        bool Init(const Descriptor &);

        VkSemaphore GetNativeHandle() const;

    private:
        friend class Device;
        Semaphore(Device &);

        VkSemaphore semaphore;
    };

    using SemaphorePtr = std::shared_ptr<Semaphore>;

} // namespace sky::drv