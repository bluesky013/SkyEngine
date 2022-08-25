//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>
#include <vulkan/CacheManager.h>

namespace sky::drv {

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
            auto setPtr = std::make_shared<DescriptorSet>(pool->device);
            setPtr->handle = set;
            setPtr->layout = layout;
            setPtr->pool = pool;
            setPtr->writeInfos.resize(layout->GetDescriptorTable().size(), {});
            return setPtr;
        };

        auto hash = layout->GetHash();
        auto iter = pool->freeList.find(hash);
        if (iter != pool->freeList.end() && !iter->second.empty()) {
            auto back = iter->second.back();
            iter->second.pop_back();
            return setCreateFn(back);
        }

        auto vl = layout->GetNativeHandle();
        VkDescriptorSetAllocateInfo setInfo = {};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.pNext = nullptr;
        setInfo.descriptorPool = pool->pool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &vl;

        VkDescriptorSet set = VK_NULL_HANDLE;
        auto result = vkAllocateDescriptorSets(pool->device.GetNativeHandle(), &setInfo, &set);
        if (result != VK_SUCCESS) {
            return {};
        }

        return setCreateFn(set);
    }

    DescriptorSetLayoutPtr DescriptorSet::GetLayout() const
    {
        return layout;
    }

    DescriptorSet::Writer& DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType type,
        const BufferPtr &buffer, VkDeviceSize offset, VkDeviceSize size)
    {
        if (!buffer) {
            return *this;
        }

        auto& bufferView = set.buffers[binding];
        auto handleFn = [&bufferView]() ->VkBuffer {
            return bufferView.buffer ? bufferView.buffer->GetNativeHandle() : VK_NULL_HANDLE;
        };

        if (handleFn() == buffer->GetNativeHandle() && bufferView.offset == offset && bufferView.size == size) {
            return *this;
        }
        bufferView.buffer = buffer;
        bufferView.offset = offset;
        bufferView.size = size;

        auto iter = updateTemplate.indices.find(binding);
        if (iter == updateTemplate.indices.end()) {
            return *this;
        }

        auto& bufferInfo = set.writeInfos[iter->second].buffer;
        bufferInfo.buffer = buffer->GetNativeHandle();
        bufferInfo.offset = offset;
        bufferInfo.range  = size;

        Write(binding, type, &bufferInfo, nullptr);
        return *this;
    }

    DescriptorSet::Writer& DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType type, const ImageViewPtr &view, const SamplerPtr &sampler)
    {
        if (!view || !sampler) {
            return *this;
        }

        auto& viewInfo = set.views[binding];
        auto viewHandleFn = [&viewInfo]() -> VkImageView {
            return viewInfo.view ? viewInfo.view->GetNativeHandle() : VK_NULL_HANDLE;
        };

        auto samplerHandleFn = [&viewInfo]() -> VkSampler {
            return viewInfo.sampler ? viewInfo.sampler->GetNativeHandle() : VK_NULL_HANDLE;
        };

        if (viewHandleFn() == view->GetNativeHandle() && samplerHandleFn() == sampler->GetNativeHandle()) {
            return *this;
        }
        viewInfo.view = view;
        viewInfo.sampler = sampler;

        auto iter = updateTemplate.indices.find(binding);
        if (iter == updateTemplate.indices.end()) {
            return *this;
        }

        auto& imageInfo = set.writeInfos[iter->second].image;
        imageInfo.sampler = sampler->GetNativeHandle();
        imageInfo.imageView = view->GetNativeHandle();

        if (type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else if (type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        Write(binding, type, nullptr, &imageInfo);
        return *this;
    }

    void DescriptorSet::Writer::Write(uint32_t binding, VkDescriptorType type,
        const VkDescriptorBufferInfo* bufferInfo,
        const VkDescriptorImageInfo* imageInfo)
    {
        set.dirty = true;
        if (updateTemplate.handle != VK_NULL_HANDLE) {
            return;
        }

        VkWriteDescriptorSet bindingInfo = {};
        bindingInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        bindingInfo.pNext = nullptr;
        bindingInfo.dstSet = set.GetNativeHandle();
        bindingInfo.dstBinding = binding;
        bindingInfo.dstArrayElement = 0;
        bindingInfo.descriptorCount = 1;
        bindingInfo.descriptorType = type;
        bindingInfo.pImageInfo = imageInfo;
        bindingInfo.pBufferInfo = bufferInfo;
        bindingInfo.pTexelBufferView = nullptr;
        writes.emplace_back(bindingInfo);
    }

    void DescriptorSet::Writer::Update()
    {
        if (!set.dirty) {
            return;
        }

        if (updateTemplate.handle == VK_NULL_HANDLE) {
            vkUpdateDescriptorSets(set.device.GetNativeHandle(), static_cast<uint32_t>(writes.size()), writes.data(),
                                   0, nullptr);
        } else {
            vkUpdateDescriptorSetWithTemplate(set.device.GetNativeHandle(), set.GetNativeHandle(), updateTemplate.handle, set.writeInfos.data());
        }
        set.dirty = false;
    }

}