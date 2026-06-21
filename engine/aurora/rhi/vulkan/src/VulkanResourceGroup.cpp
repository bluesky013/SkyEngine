//
// Aurora Vulkan ResourceGroup.
//

#include <VulkanResourceGroup.h>
#include <VulkanResourceGroupLayout.h>
#include <VulkanDevice.h>
#include <VulkanBuffer.h>
#include <VulkanImage.h>
#include <VulkanSampler.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>
#include <unordered_map>
#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanResourceGroup::VulkanResourceGroup(VulkanDevice &dev)
        : device(&dev)
    {
    }

    VulkanResourceGroup::~VulkanResourceGroup()
    {
        if (pool != VK_NULL_HANDLE) {
            device->GetDeviceFn().vkDestroyDescriptorPool(device->GetNativeHandle(), pool, nullptr);
            pool = VK_NULL_HANDLE;
            set  = VK_NULL_HANDLE;
        }
    }

    bool VulkanResourceGroup::Init(const Descriptor &desc)
    {
        if (desc.layout == nullptr) {
            LOG_E(TAG, "ResourceGroup requires a non-null layout");
            return false;
        }
        layout = static_cast<VulkanResourceGroupLayout *>(desc.layout);

        // Build a tight descriptor pool sized exactly for this layout.
        std::unordered_map<VkDescriptorType, uint32_t> counts;
        for (const auto &b : layout->GetBindings()) {
            counts[FromDescriptorType(b.type)] += b.count;
        }
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(counts.size());
        for (const auto &kv : counts) {
            sizes.push_back({kv.first, kv.second});
        }
        if (sizes.empty()) {
            // Empty layout (no bindings) — still allocate a 1-set pool with placeholder.
            sizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1});
        }

        VkDescriptorPoolCreateInfo poolCI = {};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.maxSets       = 1;
        poolCI.poolSizeCount = static_cast<uint32_t>(sizes.size());
        poolCI.pPoolSizes    = sizes.data();

        const auto &fn = device->GetDeviceFn();
        VkResult r = fn.vkCreateDescriptorPool(device->GetNativeHandle(), &poolCI, nullptr, &pool);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateDescriptorPool failed: %d", r);
            return false;
        }

        VkDescriptorSetLayout setLayout = layout->GetNativeHandle();
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &setLayout;

        r = fn.vkAllocateDescriptorSets(device->GetNativeHandle(), &allocInfo, &set);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkAllocateDescriptorSets failed: %d", r);
            return false;
        }

        return true;
    }

    void VulkanResourceGroup::Update(const std::vector<ResourceUpdateInfo> &writes)
    {
        if (writes.empty() || set == VK_NULL_HANDLE) {
            return;
        }

        std::vector<VkDescriptorBufferInfo> bufInfos;
        std::vector<VkDescriptorImageInfo>  imgInfos;
        bufInfos.reserve(writes.size());
        imgInfos.reserve(writes.size());

        std::vector<VkWriteDescriptorSet> vkWrites;
        vkWrites.reserve(writes.size());

        // Look up descriptor type from layout binding index for each write.
        const auto &bindings = layout->GetBindings();
        auto findType = [&](uint32_t b) -> VkDescriptorType {
            for (const auto &lb : bindings) {
                if (lb.binding == b) return FromDescriptorType(lb.type);
            }
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        };

        for (const auto &w : writes) {
            VkWriteDescriptorSet write = {};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = set;
            write.dstBinding      = w.binding;
            write.dstArrayElement = w.arrayElement;
            write.descriptorCount = 1;
            write.descriptorType  = findType(w.binding);

            switch (w.kind) {
            case ResourceWriteKind::BUFFER: {
                VkDescriptorBufferInfo bi = {};
                bi.buffer = w.buffer != nullptr ? static_cast<VulkanBuffer *>(w.buffer)->GetNativeHandle() : VK_NULL_HANDLE;
                bi.offset = w.bufferOffset;
                bi.range  = w.bufferRange == 0 ? VK_WHOLE_SIZE : w.bufferRange;
                bufInfos.push_back(bi);
                write.pBufferInfo = &bufInfos.back();
                break;
            }
            case ResourceWriteKind::IMAGE: {
                VkDescriptorImageInfo ii = {};
                ii.imageView   = w.image != nullptr ? static_cast<VulkanImage *>(w.image)->GetDefaultView() : VK_NULL_HANDLE;
                ii.imageLayout = FromImageLayout(w.imageLayout);
                imgInfos.push_back(ii);
                write.pImageInfo = &imgInfos.back();
                break;
            }
            case ResourceWriteKind::SAMPLER: {
                VkDescriptorImageInfo ii = {};
                ii.sampler = w.sampler != nullptr ? static_cast<VulkanSampler *>(w.sampler)->GetNativeHandle() : VK_NULL_HANDLE;
                imgInfos.push_back(ii);
                write.pImageInfo = &imgInfos.back();
                break;
            }
            case ResourceWriteKind::COMBINED_IMAGE_SAMPLER: {
                VkDescriptorImageInfo ii = {};
                ii.imageView   = w.image != nullptr ? static_cast<VulkanImage *>(w.image)->GetDefaultView() : VK_NULL_HANDLE;
                ii.imageLayout = FromImageLayout(w.imageLayout);
                ii.sampler     = w.sampler != nullptr ? static_cast<VulkanSampler *>(w.sampler)->GetNativeHandle() : VK_NULL_HANDLE;
                imgInfos.push_back(ii);
                write.pImageInfo = &imgInfos.back();
                break;
            }
            }

            vkWrites.push_back(write);
        }

        device->GetDeviceFn().vkUpdateDescriptorSets(
            device->GetNativeHandle(),
            static_cast<uint32_t>(vkWrites.size()), vkWrites.data(),
            0, nullptr);
    }

} // namespace sky::aurora
