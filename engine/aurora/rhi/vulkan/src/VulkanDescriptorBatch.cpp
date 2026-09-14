//
// Created on 2026/09/14.
//

#include <VulkanDescriptorBatch.h>
#include <VulkanBuffer.h>
#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanImage.h>
#include <VulkanResourceGroup.h>
#include <VulkanSampler.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    namespace {
        bool IsDynamic(VkDescriptorType type)
        {
            return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                   type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }
    } // namespace

    VulkanDescriptorBatch::VulkanDescriptorBatch(VulkanDevice &dev)
        : device(&dev)
    {
    }

    void VulkanDescriptorBatch::WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer,
                                            uint64_t offset, uint64_t range, uint32_t arrayElement)
    {
        auto *vkGroup = static_cast<VulkanResourceGroup *>(group);
        if (vkGroup == nullptr) {
            return;
        }
        for (const auto &b : vkGroup->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            VkDescriptorBufferInfo bi = {};
            bi.buffer = buffer != nullptr ? static_cast<VulkanBuffer *>(buffer)->GetNativeHandle() : VK_NULL_HANDLE;
            bi.offset = offset;
            if (IsDynamic(b.type) && range == 0) {
                SKY_ASSERT(false && "dynamic buffer binding requires explicit bufferRange");
                LOG_E(TAG, "dynamic buffer binding %u requires explicit range; got 0", binding);
            }
            bi.range = range == 0 ? VK_WHOLE_SIZE : range;
            mBufInfos.push_back(bi);

            VkWriteDescriptorSet w = {};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = vkGroup->set;
            w.dstBinding      = binding;
            w.dstArrayElement = arrayElement;
            w.descriptorCount = 1;
            w.descriptorType  = b.type;
            w.pBufferInfo     = &mBufInfos.back();
            mWrites.push_back(w);
            return;
        }
    }

    void VulkanDescriptorBatch::WriteImage(ResourceGroup *group, uint32_t binding, Image *image,
                                           ImageLayout layout, uint32_t arrayElement)
    {
        auto *vkGroup = static_cast<VulkanResourceGroup *>(group);
        if (vkGroup == nullptr) {
            return;
        }
        for (const auto &b : vkGroup->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            VkDescriptorImageInfo ii = {};
            ii.sampler     = VK_NULL_HANDLE;
            ii.imageView   = image != nullptr ? static_cast<VulkanImage *>(image)->GetDefaultView() : VK_NULL_HANDLE;
            ii.imageLayout = FromImageLayout(layout);
            mImgInfos.push_back(ii);

            VkWriteDescriptorSet w = {};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = vkGroup->set;
            w.dstBinding      = binding;
            w.dstArrayElement = arrayElement;
            w.descriptorCount = 1;
            w.descriptorType  = b.type;
            w.pImageInfo      = &mImgInfos.back();
            mWrites.push_back(w);
            return;
        }
    }

    void VulkanDescriptorBatch::WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        auto *vkGroup = static_cast<VulkanResourceGroup *>(group);
        if (vkGroup == nullptr) {
            return;
        }
        for (const auto &b : vkGroup->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            VkDescriptorImageInfo ii = {};
            ii.sampler     = sampler != nullptr ? static_cast<VulkanSampler *>(sampler)->GetNativeHandle() : VK_NULL_HANDLE;
            ii.imageView   = VK_NULL_HANDLE;
            ii.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            mImgInfos.push_back(ii);

            VkWriteDescriptorSet w = {};
            w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet          = vkGroup->set;
            w.dstBinding      = binding;
            w.dstArrayElement = arrayElement;
            w.descriptorCount = 1;
            w.descriptorType  = b.type;
            w.pImageInfo      = &mImgInfos.back();
            mWrites.push_back(w);
            return;
        }
    }

    void VulkanDescriptorBatch::Flush()
    {
        if (mWrites.empty()) {
            return;
        }
        device->GetDeviceFn().vkUpdateDescriptorSets(device->GetNativeHandle(),
                                                     static_cast<uint32_t>(mWrites.size()), mWrites.data(), 0, nullptr);
        Reset();
    }

    void VulkanDescriptorBatch::Reset()
    {
        mWrites.clear();
        mBufInfos.clear();
        mImgInfos.clear();
    }

} // namespace sky::aurora
