//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorBatch.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    // Frame-scoped cross-set descriptor write accumulator. Write* appends
    // VkWriteDescriptorSet entries targeting each group's current set; Flush()
    // issues a single vkUpdateDescriptorSets; Reset() clears for reuse.
    class VulkanDescriptorBatch : public DescriptorBatch {
    public:
        explicit VulkanDescriptorBatch(VulkanDevice &dev);
        ~VulkanDescriptorBatch() override = default;

        void WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        void WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                        ImageLayout layout, uint32_t arrayElement = 0) override;
        void WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler,
                          uint32_t arrayElement = 0) override;

        void Flush() override;
        void Reset() override;

    private:
        VulkanDevice *device = nullptr;

        std::vector<VkWriteDescriptorSet>   mWrites;
        std::vector<VkDescriptorBufferInfo> mBufInfos;
        std::vector<VkDescriptorImageInfo>  mImgInfos;
    };

} // namespace sky::aurora
