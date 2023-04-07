//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/CacheManager.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>
#include <vulkan/Util.h>

namespace sky::vk {

    DescriptorSet::~DescriptorSet()
    {
        if (pool) {
            pool->Free(*this);
        }
    }

    VkDescriptorSet DescriptorSet::GetNativeHandle() const
    {
        return handle;
    }

    DescriptorSet::Writer DescriptorSet::CreateWriter()
    {
        return {*this, layout->GetUpdateTemplate()};
    }

    DescriptorSetPtr DescriptorSet::Allocate(const DescriptorSetPoolPtr &pool, const DescriptorSetLayoutPtr &layout)
    {
        auto setCreateFn = [&pool, &layout](VkDescriptorSet set) {
            auto setPtr    = std::make_shared<DescriptorSet>(pool->device);
            setPtr->handle = set;
            setPtr->layout = layout;
            setPtr->pool   = pool;
            setPtr->Setup();
            return setPtr;
        };

        auto hash = layout->GetHash();
        auto iter = pool->freeList.find(hash);
        if (iter != pool->freeList.end() && !iter->second.empty()) {
            auto back = iter->second.back();
            iter->second.pop_back();
            return setCreateFn(back);
        }

        auto                        vl      = layout->GetNativeHandle();
        VkDescriptorSetAllocateInfo setInfo = {};
        setInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.descriptorPool              = pool->pool;
        setInfo.descriptorSetCount          = 1;
        setInfo.pSetLayouts                 = &vl;

        auto &variableDescriptorCounts = layout->GetVariableDescriptorCounts();
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = {};
        if (!variableDescriptorCounts.empty()) {
            variableDescriptorCountAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
            variableDescriptorCountAllocInfo.descriptorSetCount = 1;
            variableDescriptorCountAllocInfo.pDescriptorCounts  = variableDescriptorCounts.data();
            setInfo.pNext                       = &variableDescriptorCountAllocInfo;
        }

        VkDescriptorSet set    = VK_NULL_HANDLE;
        auto            result = vkAllocateDescriptorSets(pool->device.GetNativeHandle(), &setInfo, &set);
        if (result != VK_SUCCESS) {
            return {};
        }

        return setCreateFn(set);
    }

    DescriptorSetLayoutPtr DescriptorSet::GetLayout() const
    {
        return layout;
    }

    bool DescriptorSet::Init(const Descriptor &desc)
    {
        return true;
    }

    void DescriptorSet::Setup()
    {
        auto &table      = layout->GetDescriptorTable();
        auto &indicesMap = layout->GetUpdateTemplate();

        writeInfos.resize(layout->GetDescriptorNum(), {});
        for (auto &[binding, info] : table) {
            writeEntries.emplace_back(VkWriteDescriptorSet{});
            VkWriteDescriptorSet &entry = writeEntries.back();
            entry.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            entry.pNext                 = nullptr;
            entry.dstSet                = handle;
            entry.dstBinding            = binding;
            entry.dstArrayElement       = 0;
            entry.descriptorCount       = info.descriptorCount;
            entry.descriptorType        = info.descriptorType;

            uint32_t offset = indicesMap.indices.at(binding);
            for (uint32_t i = 0; i < entry.descriptorCount; ++i) {
                uint32_t index = offset + i;

                if (IsBufferDescriptor(info.descriptorType)) {
                    entry.pBufferInfo = &writeInfos[index].buffer;
                } else if (IsImageDescriptor(info.descriptorType)) {
                    auto &imageInfo = writeInfos[index].image;
                    if (info.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER || info.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE) {
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    } else if (info.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    }
                    entry.pImageInfo = &imageInfo;
                } else {
                    entry.pTexelBufferView = &writeInfos[index].bufferView;
                }
            }
        }
    }

    DescriptorSet::Writer &
    DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType type, const BufferPtr &buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t index)
    {
        if (!buffer) {
            return *this;
        }

        auto &bufferView = set.buffers[binding];
        auto  handleFn   = [&bufferView]() -> VkBuffer { return bufferView.buffer ? bufferView.buffer->GetNativeHandle() : VK_NULL_HANDLE; };

        if (handleFn() == buffer->GetNativeHandle() && bufferView.offset == offset && bufferView.size == size) {
            return *this;
        }
        bufferView.buffer = buffer;
        bufferView.offset = offset;
        bufferView.size   = size;

        auto iter = updateTemplate.indices.find(binding);
        if (iter == updateTemplate.indices.end()) {
            return *this;
        }

        auto &bufferInfo  = set.writeInfos[iter->second + index].buffer;
        bufferInfo.buffer = buffer->GetNativeHandle();
        bufferInfo.offset = offset;
        bufferInfo.range  = size;
        set.dirty         = true;
        return *this;
    }

    DescriptorSet::Writer &DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType type, const ImageViewPtr &view, const SamplerPtr &sampler, uint32_t index)
    {
        auto &viewInfo     = set.imageViews[binding];
        auto viewHandleFn = [](const ImageViewPtr &view) -> VkImageView { return view ? view->GetNativeHandle() : VK_NULL_HANDLE; };
        auto samplerHandleFn = [](const SamplerPtr &sampler) -> VkSampler { return sampler ? sampler->GetNativeHandle() : VK_NULL_HANDLE; };

        if (viewInfo.view.get() == view.get() && viewInfo.sampler.get() == sampler.get()) {
            return *this;
        }

        viewInfo.view    = view;
        viewInfo.sampler = sampler;

        auto iter = updateTemplate.indices.find(binding);
        if (iter == updateTemplate.indices.end()) {
            return *this;
        }

        auto &imageInfo     = set.writeInfos[iter->second + index].image;
        imageInfo.sampler   = samplerHandleFn(sampler);
        imageInfo.imageView = viewHandleFn(view);

        set.dirty = true;
        return *this;
    }

    DescriptorSet::Writer &DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType, const BufferViewPtr &view, uint32_t index)
    {
        auto &viewInfo     = set.bufferViews[binding];
        if (viewInfo.get() == view.get()) {
            return *this;
        }

        viewInfo = view;
        auto iter = updateTemplate.indices.find(binding);
        if (iter == updateTemplate.indices.end()) {
            return *this;
        }

        set.writeInfos[iter->second + index].bufferView = view->GetNativeHandle();
        set.dirty = true;
        return *this;
    }

    void DescriptorSet::Writer::Update()
    {
        if (!set.dirty) {
            return;
        }

        if (updateTemplate.handle == VK_NULL_HANDLE) {
            vkUpdateDescriptorSets(set.device.GetNativeHandle(), static_cast<uint32_t>(set.writeEntries.size()), set.writeEntries.data(), 0, nullptr);
        } else {
            vkUpdateDescriptorSetWithTemplate(set.device.GetNativeHandle(), set.GetNativeHandle(), updateTemplate.handle, set.writeInfos.data());
        }
        set.dirty = false;
    }

} // namespace sky::vk
