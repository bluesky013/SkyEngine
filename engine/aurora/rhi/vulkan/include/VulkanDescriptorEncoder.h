#pragma once

#include <VulkanResourceGroup.h>
#include <aurora/rhi/DescriptorEncoder.h>

#include <deque>

namespace sky::aurora {
    class VulkanDescriptorEncoder : public DescriptorEncoder {
    public:
        explicit VulkanDescriptorEncoder(VulkanResourceGroup &group);
        ~VulkanDescriptorEncoder() override = default;
        bool               WriteBuffer(uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t arrayElement = 0) override;
        bool               WriteImage(uint32_t binding, Image *image, ImageLayout layout, uint32_t arrayElement = 0) override;
        bool               WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement = 0) override;
        bool               End() override;
        const std::string &LastError() const
        {
            return error;
        }

    private:
        bool                                            Fail(const char *message);
        bool                                            Ready();
        void                                            AppendWrites(std::vector<VkWriteDescriptorSet> &writes);
        void                                            Commit();
        const VulkanPlannedBinding                     *Find(uint32_t binding, uint32_t element);
        CounterPtr<VulkanResourceGroup>                 group;
        std::vector<DescriptorWriteInfo>                infos;
        std::vector<bool>                               assigned;
        std::vector<BufferPtr>                          buffers;
        std::vector<ImagePtr>                           images;
        std::vector<SamplerPtr>                         samplers;
        std::deque<std::vector<VkDescriptorBufferInfo>> bufferWrites;
        std::deque<std::vector<VkDescriptorImageInfo>>  imageWrites;
        bool                                            ended = false;
        std::string                                     error;
    };
} // namespace sky::aurora
