//
// Created on 2026/09/14.
//

#include <VulkanDescriptorEncoder.h>
#include <VulkanResourceGroup.h>
#include <VulkanBuffer.h>
#include <VulkanImage.h>
#include <VulkanSampler.h>
#include <VulkanDevice.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    namespace {
        bool IsDynamic(VkDescriptorType type)
        {
            return type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                   type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        }

        bool IsImage(VkDescriptorType type)
        {
            return type == VK_DESCRIPTOR_TYPE_SAMPLER ||
                   type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                   type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
                   type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE ||
                   type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
    } // namespace

    VulkanDescriptorEncoder::VulkanDescriptorEncoder(VulkanResourceGroup &rg)
        : group(&rg)
    {
    }

    void VulkanDescriptorEncoder::WriteBuffer(uint32_t binding, Buffer *buffer,
                                              uint64_t offset, uint64_t range,
                                              uint32_t arrayElement)
    {
        for (const auto &b : group->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            const uint32_t slot = b.slotBase + arrayElement;
            auto &wi            = group->mWriteInfos[slot];
            wi.buffer.buffer    = buffer != nullptr ? static_cast<VulkanBuffer *>(buffer)->GetNativeHandle() : VK_NULL_HANDLE;
            wi.buffer.offset    = offset;
            if (IsDynamic(b.type) && range == 0) {
                SKY_ASSERT(false && "dynamic buffer binding requires explicit bufferRange");
                LOG_E(TAG, "dynamic buffer binding %u requires explicit range; got 0", binding);
            }
            wi.buffer.range = range == 0 ? VK_WHOLE_SIZE : range;
            group->mDirty   = true;
            return;
        }
    }

    void VulkanDescriptorEncoder::WriteImage(uint32_t binding, Image *image,
                                             ImageLayout layout, uint32_t arrayElement)
    {
        for (const auto &b : group->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            const uint32_t slot = b.slotBase + arrayElement;
            auto &wi            = group->mWriteInfos[slot];
            wi.image.sampler     = VK_NULL_HANDLE;
            wi.image.imageView   = image != nullptr ? static_cast<VulkanImage *>(image)->GetDefaultView() : VK_NULL_HANDLE;
            wi.image.imageLayout = FromImageLayout(layout);
            group->mDirty        = true;
            return;
        }
    }

    void VulkanDescriptorEncoder::WriteSampler(uint32_t binding, Sampler *sampler, uint32_t arrayElement)
    {
        for (const auto &b : group->mBindings) {
            if (b.binding != binding) {
                continue;
            }
            const uint32_t slot = b.slotBase + arrayElement;
            auto &wi            = group->mWriteInfos[slot];
            wi.image.sampler     = sampler != nullptr ? static_cast<VulkanSampler *>(sampler)->GetNativeHandle() : VK_NULL_HANDLE;
            wi.image.imageView   = VK_NULL_HANDLE;
            wi.image.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            group->mDirty        = true;
            return;
        }
    }

    void VulkanDescriptorEncoder::End()
    {
        if (!group->mDirty || group->set == VK_NULL_HANDLE) {
            group->mDirty = false;
            return;
        }

        auto *device = group->device;
        VkDescriptorUpdateTemplate tpl = group->shader->GetDescriptorUpdateTemplate(group->mSetIndex);
        if (tpl != VK_NULL_HANDLE) {
            device->GetDeviceFn().vkUpdateDescriptorSetWithTemplate(
                device->GetNativeHandle(), group->set, tpl, group->mWriteInfos.data());
        } else {
            // fallback (template creation failed): rebuild VkWriteDescriptorSet
            std::vector<VkWriteDescriptorSet> writes;
            writes.reserve(group->mBindings.size());
            for (const auto &b : group->mBindings) {
                VkWriteDescriptorSet w = {};
                w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                w.dstSet          = group->set;
                w.dstBinding      = b.binding;
                w.dstArrayElement = 0;
                w.descriptorCount = b.count;
                w.descriptorType  = b.type;
                if (IsImage(b.type)) {
                    w.pImageInfo = reinterpret_cast<const VkDescriptorImageInfo *>(&group->mWriteInfos[b.slotBase]);
                } else {
                    w.pBufferInfo = reinterpret_cast<const VkDescriptorBufferInfo *>(&group->mWriteInfos[b.slotBase]);
                }
                writes.push_back(w);
            }
            device->GetDeviceFn().vkUpdateDescriptorSets(
                device->GetNativeHandle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }

        group->mDirty = false;
    }

} // namespace sky::aurora
