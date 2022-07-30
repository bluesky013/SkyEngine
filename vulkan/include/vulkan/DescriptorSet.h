//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/ImageView.h>
#include <vulkan/Sampler.h>
#include <vulkan/Buffer.h>
#include <vector>
#include <list>
#include <memory>

namespace sky::drv {
    class DescriptorSet : public DevObject {
    public:
        DescriptorSet(Device& device) : DevObject(device) {}
        ~DescriptorSet();

        VkDescriptorSet GetNativeHandle() const;

        struct Writer {
        public:
            Writer(DescriptorSet& s) : set(s)
            {
            }

            Writer& Write(uint32_t binding, VkDescriptorType, BufferPtr buffer, VkDeviceSize offset, VkDeviceSize size);

            Writer& Write(uint32_t binding, VkDescriptorType, ImageViewPtr view, SamplerPtr sampler);

            void Update();

        private:
            void Write(uint32_t binding, VkDescriptorType type,
                const VkDescriptorBufferInfo* bufferInfo,
                const VkDescriptorImageInfo* imageInfo);

            std::list<VkDescriptorBufferInfo> buffers;
            std::list<VkDescriptorImageInfo> images;
            std::vector<VkWriteDescriptorSet> writes;
            DescriptorSet& set;
        };

        Writer CreateWriter();

        static std::shared_ptr<DescriptorSet> Allocate(DescriptorSetPoolPtr pool, DescriptorSetLayoutPtr layout);

    private:
        friend class DescriptorSetPool;
        DescriptorSetLayoutPtr layout;
        DescriptorSetPoolPtr pool;
        VkDescriptorSet handle = VK_NULL_HANDLE;
        std::unordered_map<uint32_t, BufferPtr> buffers;
        std::unordered_map<uint32_t, ImageViewPtr> views;
        std::unordered_map<uint32_t, SamplerPtr> samplers;
    };

    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

}