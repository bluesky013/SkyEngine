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
#include <rhi/DescriptorSet.h>

namespace sky::vk {

    class DescriptorSet : public rhi::DescriptorSet, public DevObject {
    public:
        explicit DescriptorSet(Device &device) : DevObject(device) {}
        ~DescriptorSet() override;

        VkDescriptorSet GetNativeHandle() const;
        uint32_t GetDescriptorNum() const { return layout->GetDescriptorNum(); }

        static std::shared_ptr<DescriptorSet> Allocate(const DescriptorSetPoolPtr &pool, const DescriptorSetLayoutPtr &layout);
        DescriptorSetLayoutPtr GetLayout() const { return layout; }

        void BindBuffer(uint32_t binding, const rhi::BufferView &view, uint32_t index) override;
        void BindBuffer(uint32_t binding, const rhi::BufferPtr &buffer, uint64_t offset, uint64_t size, uint32_t index) override;
        void BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index, rhi::DescriptorBindFlags flags) override;
        void BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index) override;
        void Update() override;

    private:
        friend class Device;
        friend class DescriptorSetPool;

        void Setup();

        DescriptorSetLayoutPtr layout;
        DescriptorSetPoolPtr   pool;
        VkDescriptorSet        handle = VK_NULL_HANDLE;
        VkDescriptorPool       poolHandle = VK_NULL_HANDLE;
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

        std::vector<DescriptorWriteInfo>  writeInfos;
        std::vector<VkWriteDescriptorSet> writeEntries;
    };

    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

} // namespace sky::vk
