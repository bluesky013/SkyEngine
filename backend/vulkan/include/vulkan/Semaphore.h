//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <rhi/Semaphore.h>

namespace sky::vk {

    class Device;

    class Semaphore : public rhi::Semaphore, public DevObject {
    public:
        ~Semaphore();

        struct VkDescriptor {};

        bool Init(const Descriptor &desc);
        bool Init(const VkDescriptor &);

        VkSemaphore GetNativeHandle() const;

    private:
        friend class Device;
        Semaphore(Device &);

        VkSemaphore semaphore;
    };

    using SemaphorePtr = std::shared_ptr<Semaphore>;

} // namespace sky::vk
