//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Semaphore.h"
#include "core/logger/Logger.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
static const char *TAG = "Vulkan";

namespace sky::vk {

    Semaphore::Semaphore(Device &dev) : DevObject(dev), semaphore(VK_NULL_HANDLE)
    {
    }

    Semaphore::~Semaphore()
    {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(device.GetNativeHandle(), semaphore, VKL_ALLOC);
        }
    }

    bool Semaphore::Init(const Descriptor &desc)
    {
        return Init(VkDescriptor{});
    }

    bool Semaphore::Init(const VkDescriptor &des)
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        auto rst                            = vkCreateSemaphore(device.GetNativeHandle(), &semaphoreInfo, VKL_ALLOC, &semaphore);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create semaphore failed, %d", rst);
            return false;
        }
        return true;
    }

    VkSemaphore Semaphore::GetNativeHandle() const
    {
        return semaphore;
    }
} // namespace sky::vk
