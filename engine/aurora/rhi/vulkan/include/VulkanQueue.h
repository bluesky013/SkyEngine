//
// Aurora Vulkan Queue.
//

#pragma once

#include <aurora/rhi/Queue.h>
#include <VulkanFunctions.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanQueue : public Queue {
    public:
        VulkanQueue(VulkanDevice &dev, QueueType type, VkQueue native, uint32_t familyIndex);
        ~VulkanQueue() override = default;

        void      Submit(const SubmitInfo &info) override;
        void      WaitIdle() override;
        QueueType GetType() const override { return type; }

        VkQueue  GetNativeHandle() const { return queue; }
        uint32_t GetFamilyIndex() const { return familyIndex; }

    private:
        VulkanDevice &device;
        QueueType     type;
        VkQueue       queue       = VK_NULL_HANDLE;
        uint32_t      familyIndex = 0;
    };

} // namespace sky::aurora
