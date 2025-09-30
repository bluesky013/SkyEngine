//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/CacheManager.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>
#include <vulkan/Util.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    DescriptorSet::~DescriptorSet()
    {
        if (pool) {
            pool->Free(*this, poolHandle);
        }
    }

    VkDescriptorSet DescriptorSet::GetNativeHandle() const
    {
        return handle;
    }

    DescriptorSetPtr DescriptorSet::Allocate(const DescriptorSetPoolPtr &pool, const DescriptorSetLayoutPtr &layout)
    {
        auto setCreateFn = [&pool, &layout](const std::pair<VkDescriptorSet, VkDescriptorPool> &pair) {
            auto setPtr    = std::make_shared<DescriptorSet>(pool->device);
            setPtr->handle = pair.first;
            setPtr->poolHandle = pair.second;
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

        auto &device = pool->GetDevice();
        auto  allocateFn = [&device, &layout](VkDescriptorPool pool) {
            auto *vl = layout->GetNativeHandle();

            VkDescriptorSetAllocateInfo setInfo = {};
            setInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            setInfo.descriptorPool              = pool;
            setInfo.descriptorSetCount          = 1;
            setInfo.pSetLayouts                 = &vl;

            const auto &variableDescriptorCounts = layout->GetVariableDescriptorCounts();
            VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo = 
            {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO
            };

            if (!variableDescriptorCounts.empty()) {
                variableDescriptorCountAllocInfo.descriptorSetCount = 1;
                variableDescriptorCountAllocInfo.pDescriptorCounts  = variableDescriptorCounts.data();
                setInfo.pNext                                       = &variableDescriptorCountAllocInfo;
            }

            VkDescriptorSet set = VK_NULL_HANDLE;
            auto result = vkAllocateDescriptorSets(device.GetNativeHandle(), &setInfo, &set);
            return result == VK_SUCCESS ? set : VK_NULL_HANDLE;
        };

        auto poolHandle = pool->GetCurrentPool();
        auto set        = allocateFn(poolHandle);
        if (set == VK_NULL_HANDLE) {
            poolHandle = pool->CreateNewNativePool();
            set        = allocateFn(poolHandle);
        }

        if (set == VK_NULL_HANDLE) {
            return {};
        }

        return setCreateFn({set, poolHandle});
    }

    void DescriptorSet::Setup()
    {
        const auto &bindings = layout->GetBindings();

        writeInfos.resize(layout->GetDescriptorNum(), {});
        for (const auto &binding : bindings) {
            writeEntries.emplace_back(VkWriteDescriptorSet{});
            VkWriteDescriptorSet &entry = writeEntries.back();
            entry.sType                 = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            entry.pNext                 = nullptr;
            entry.dstSet                = handle;
            entry.dstBinding            = binding.binding;
            entry.dstArrayElement       = 0;
            entry.descriptorCount       = binding.count;
            entry.descriptorType        = FromRHI(binding.type);

            uint32_t offset = layout->GetDescriptorSetOffsetByBinding(binding.binding);
            for (uint32_t i = 0; i < entry.descriptorCount; ++i) {
                uint32_t index = offset + i;

                if (IsBufferDescriptor(entry.descriptorType)) {
                    entry.pBufferInfo = &writeInfos[index].buffer;
                } else if (IsImageDescriptor(entry.descriptorType)) {
                    auto &imageInfo = writeInfos[index].image;
                    if (entry.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                        entry.descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                        entry.descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                    } else if (entry.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                    }
                    entry.pImageInfo = &imageInfo;
                } else {
                    entry.pTexelBufferView = &writeInfos[index].bufferView;
                }
            }
        }
    }

    void DescriptorSet::BindBuffer(uint32_t binding, const rhi::BufferView &view, uint32_t index)
    {
        auto vkBuffer = std::static_pointer_cast<Buffer>(view.buffer);
        auto &writeInfo = writeInfos[layout->GetDescriptorSetOffsetByBinding(binding) + index];
        if (writeInfo.buffer.buffer != vkBuffer->GetNativeHandle() ||
            writeInfo.buffer.offset != view.offset ||
            writeInfo.buffer.range != view.range) {
            writeInfo.buffer.buffer = vkBuffer->GetNativeHandle();
            writeInfo.buffer.range  = view.range;
            writeInfo.buffer.offset = view.offset;
            dirty = true;
        }
    }

    void DescriptorSet::BindBuffer(uint32_t binding, const rhi::BufferPtr &buffer, uint64_t offset, uint64_t size, uint32_t index)
    {
        auto vkBuffer = std::static_pointer_cast<Buffer>(buffer);
        auto &writeInfo = writeInfos[layout->GetDescriptorSetOffsetByBinding(binding) + index];
        if (writeInfo.buffer.buffer != vkBuffer->GetNativeHandle() ||
            writeInfo.buffer.offset != offset ||
            writeInfo.buffer.range != size) {
            writeInfo.buffer.buffer = vkBuffer->GetNativeHandle();
            writeInfo.buffer.range  = size;
            writeInfo.buffer.offset = offset;

            dirty = true;
        }
    }

    void DescriptorSet::BindImageView(uint32_t binding, const rhi::ImageViewPtr &view, uint32_t index, rhi::DescriptorBindFlags flags)
    {
        auto vkImageView = std::static_pointer_cast<ImageView>(view);
        uint32_t writeIndex = layout->GetDescriptorSetOffsetByBinding(binding) + index;
        SKY_ASSERT(writeIndex < writeInfos.size());

        auto &writeInfo = writeInfos[writeIndex];
        const auto &viewInfo = vkImageView->GetSubRange();

        if (writeInfo.image.sampler == VK_NULL_HANDLE) {
            writeInfo.image.sampler = device.GetDefaultSampler()->GetNativeHandle();
        }
        writeInfo.image.imageView = vkImageView->GetNativeHandle();

        SKY_ASSERT(binding < writeEntries.size());
        const auto &entry = writeEntries[binding];
        auto mask = view->GetViewDesc().subRange.aspectMask;
        auto descriptorType = entry.descriptorType;

        if (descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
            descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
            descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
//            if (mask & (rhi::AspectFlagBit::DEPTH_BIT | rhi::AspectFlagBit::STENCIL_BIT)) {
//                writeInfo.image.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
//            } else {
                writeInfo.image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//            }
        } else if (descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
            writeInfo.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        if (flags & rhi::DescriptorBindFlagBit::FEEDBACK_LOOP) {
            writeInfo.image.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // VK_IMAGE_LAYOUT_ATTACHMENT_FEEDBACK_LOOP_OPTIMAL_EXT
        }
        dirty = true;
    }

    void DescriptorSet::BindSampler(uint32_t binding, const rhi::SamplerPtr &sampler, uint32_t index)
    {
        auto vkSampler = std::static_pointer_cast<Sampler>(sampler);
        auto &writeInfo = writeInfos[layout->GetDescriptorSetOffsetByBinding(binding) + index];
        if (writeInfo.image.sampler == vkSampler->GetNativeHandle()) {
            return;
        }
        writeInfo.image.sampler = vkSampler->GetNativeHandle();
        dirty = true;
    }

    void DescriptorSet::Update()
    {
        if (!dirty) {
            return;
        }

        auto *updateTemplate = layout->GetUpdateTemplate();
        if (updateTemplate == VK_NULL_HANDLE) {
            vkUpdateDescriptorSets(device.GetNativeHandle(), static_cast<uint32_t>(writeEntries.size()), writeEntries.data(), 0, nullptr);
        } else {
            vkUpdateDescriptorSetWithTemplate(device.GetNativeHandle(), handle, updateTemplate, writeInfos.data());
        }
        dirty = false;
    }
} // namespace sky::vk
