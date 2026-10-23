#pragma once

#include <VulkanDescriptorEncoder.h>
#include <aurora/rhi/DescriptorBatch.h>

#include <map>

namespace sky::aurora {
    class VulkanDescriptorBatch : public DescriptorBatch {
    public:
        explicit VulkanDescriptorBatch(VulkanDevice &device);
        ~VulkanDescriptorBatch() override = default;
        bool WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override;
        bool Flush() override;
        void Reset() override;

    private:
        VulkanDescriptorEncoder                                                  *Find(ResourceGroup *group);
        VulkanDevice                                                             *device = nullptr;
        std::map<VulkanResourceGroup *, std::unique_ptr<VulkanDescriptorEncoder>> encoders;
        bool                                                                      failed = false;
    };
} // namespace sky::aurora
