//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vulkan/CommandBuffer.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::vk {

    class Device;

    class CommandPool : public DevObject {
    public:
        ~CommandPool() override;

        struct VkDescriptor {
            uint32_t                 queueFamilyIndex = 0;
            VkCommandPoolCreateFlags flag             = 0;
        };

        bool Init(const VkDescriptor &);

        CommandBufferPtr Allocate(const CommandBuffer::VkDescriptor &);

    private:
        friend class Device;
        explicit CommandPool(Device &);

        VkCommandPool pool;
    };

    using CommandPoolPtr = std::shared_ptr<CommandPool>;
} // namespace sky::vk
