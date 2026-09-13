//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rhi/DescriptorEncoder.h>

namespace sky::aurora {

    class VulkanResourceGroup;

    // Thin facade over a VulkanResourceGroup's persistent packed write buffer.
    // Write* fills mWriteInfos + sets dirty; End() flushes via the set's
    // VkDescriptorUpdateTemplate (falls back to vkUpdateDescriptorSets).
    class VulkanDescriptorEncoder : public DescriptorEncoder {
    public:
        explicit VulkanDescriptorEncoder(VulkanResourceGroup &group);
        ~VulkanDescriptorEncoder() override = default;

        void WriteBuffer(uint32_t binding, Buffer *buffer,
                         uint64_t offset, uint64_t range,
                         uint32_t arrayElement = 0) override;
        void WriteImage(uint32_t binding, Image *image,
                        ImageLayout layout,
                        uint32_t arrayElement = 0) override;
        void WriteSampler(uint32_t binding, Sampler *sampler,
                          uint32_t arrayElement = 0) override;
        void End() override;

    private:
        VulkanResourceGroup *group;
    };

} // namespace sky::aurora
