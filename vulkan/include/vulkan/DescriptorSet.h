//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <list>
#include <memory>
#include <vector>
#include <vulkan/Buffer.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/DevObject.h>
#include <vulkan/ImageView.h>
#include <vulkan/Sampler.h>
#include <vulkan/BufferView.h>

namespace sky::drv {

    class DescriptorSet : public DevObject {
    public:
        DescriptorSet(Device &device) : DevObject(device)
        {
        }
        ~DescriptorSet();

        VkDescriptorSet GetNativeHandle() const;

        struct Writer {
        public:
            Writer(DescriptorSet &s, const UpdateTemplate &u) : set(s), updateTemplate(u)
            {
            }

            Writer &Write(uint32_t binding, VkDescriptorType, const BufferPtr &buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t index = 0);

            Writer &Write(uint32_t binding, VkDescriptorType, const ImageViewPtr &view, const SamplerPtr &sampler, uint32_t index = 0);

            Writer &Write(uint32_t binding, VkDescriptorType, const BufferViewPtr &view, uint32_t index = 0);

            void Update();

        private:
            void                  Write(uint32_t                      index,
                                        uint32_t                      binding,
                                        VkDescriptorType              type,
                                        const VkDescriptorBufferInfo *bufferInfo,
                                        const VkDescriptorImageInfo  *imageInfo);
            DescriptorSet        &set;
            const UpdateTemplate &updateTemplate;
        };

        Writer CreateWriter();

        static std::shared_ptr<DescriptorSet> Allocate(const DescriptorSetPoolPtr &pool, const DescriptorSetLayoutPtr &layout);

        DescriptorSetLayoutPtr GetLayout() const;

    private:
        friend class DescriptorSetPool;
        void Setup();

        DescriptorSetLayoutPtr layout;
        DescriptorSetPoolPtr   pool;
        VkDescriptorSet        handle = VK_NULL_HANDLE;
        bool                   dirty  = false;
        struct BufferSubResource {
            BufferPtr    buffer;
            VkDeviceSize size   = 0;
            VkDeviceSize offset = 0;
        };

        struct ImageSampler {
            ImageViewPtr view;
            SamplerPtr   sampler;
        };

        std::unordered_map<uint32_t, BufferSubResource> buffers;
        std::unordered_map<uint32_t, ImageSampler>      imageViews;
        std::unordered_map<uint32_t, BufferViewPtr>     bufferViews;
        std::vector<DescriptorWriteInfo>                writeInfos;
        std::vector<VkWriteDescriptorSet>               writeEntries;
    };

    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

} // namespace sky::drv
