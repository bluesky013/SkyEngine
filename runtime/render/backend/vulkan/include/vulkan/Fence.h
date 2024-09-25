//
// Created by Zach Lee on 2021/11/7.
//
#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "rhi/Fence.h"

namespace sky::vk {

    class Device;

    class Fence : public rhi::Fence, public DevObject {
    public:
        ~Fence();

        struct VkDescriptor {
            VkFenceCreateFlags flag = 0;
        };

        void Wait() override;
        void Reset() override;

        VkFence GetNativeHandle() const;

    private:
        friend class Device;
        Fence(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkFence fence;
    };

    using FencePtr = std::shared_ptr<Fence>;

} // namespace sky::vk
