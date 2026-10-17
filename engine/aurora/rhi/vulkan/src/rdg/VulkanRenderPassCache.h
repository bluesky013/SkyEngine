#pragma once

#include <common/FrameObjectCache.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {
    class VulkanDevice;
    struct VulkanCachedRenderPass {
        explicit VulkanCachedRenderPass(VulkanDevice &device) : device(device)
        {
        }
        ~VulkanCachedRenderPass();
        VulkanDevice &device;
        VkRenderPass  handle   = VK_NULL_HANDLE;
        uint64_t      identity = 0;
    };
    class VulkanRenderPassCache final {
    public:
        explicit VulkanRenderPassCache(VulkanDevice &device) : device(device)
        {
        }
        std::variant<FrameError, std::shared_ptr<VulkanCachedRenderPass>> Acquire(const VkRenderPassCreateInfo2 &, uint64_t epoch);
        void                                                              Clear()
        {
            cache.Clear();
        }

    private:
        VulkanDevice                                                                    &device;
        uint64_t                                                                         nextIdentity = 1;
        FrameObjectCache<std::vector<uint64_t>, std::shared_ptr<VulkanCachedRenderPass>> cache;
    };
} // namespace sky::aurora
