#include <VulkanDescriptorBatch.h>

#include <VulkanDevice.h>
#include <core/logger/Logger.h>

namespace sky::aurora {
    VulkanDescriptorBatch::VulkanDescriptorBatch(VulkanDevice &dev) : device(&dev)
    {
    }
    VulkanDescriptorEncoder *VulkanDescriptorBatch::Find(ResourceGroup *group)
    {
        if (failed) {
            return nullptr;
        }
        if (group == nullptr || group->GetDevice() != device) {
            failed = true;
            LOG_E("AuroraVulkan", "Descriptor batch resource group belongs to an unknown or different device");
            return nullptr;
        }
        if (group->IsParameterSnapshot()) {
            failed = true;
            LOG_E("AuroraVulkan", "Descriptor batch cannot update an immutable parameter snapshot");
            return nullptr;
        }
        auto *native  = static_cast<VulkanResourceGroup *>(group);
        auto &encoder = encoders[native];
        if (!encoder) {
            encoder = std::make_unique<VulkanDescriptorEncoder>(*native);
        }
        return encoder.get();
    }
    bool VulkanDescriptorBatch::WriteBuffer(ResourceGroup *group, uint32_t binding, Buffer *buffer, uint64_t offset, uint64_t range, uint32_t element)
    {
        auto *encoder = Find(group);
        if (encoder == nullptr || !encoder->WriteBuffer(binding, buffer, offset, range, element)) {
            failed = true;
            return false;
        }
        return true;
    }
    bool VulkanDescriptorBatch::WriteImage(ResourceGroup *group, uint32_t binding, Image *image, ImageLayout layout, uint32_t element)
    {
        auto *encoder = Find(group);
        if (encoder == nullptr || !encoder->WriteImage(binding, image, layout, element)) {
            failed = true;
            return false;
        }
        return true;
    }
    bool VulkanDescriptorBatch::WriteSampler(ResourceGroup *group, uint32_t binding, Sampler *sampler, uint32_t element)
    {
        auto *encoder = Find(group);
        if (encoder == nullptr || !encoder->WriteSampler(binding, sampler, element)) {
            failed = true;
            return false;
        }
        return true;
    }
    bool VulkanDescriptorBatch::Flush()
    {
        if (failed) {
            return false;
        }
        for (auto &[group, encoder] : encoders) {
            if (!encoder->Ready()) {
                failed = true;
                return false;
            }
        }
        std::vector<VkWriteDescriptorSet> writes;
        for (auto &[group, encoder] : encoders) {
            encoder->AppendWrites(writes);
        }
        if (!writes.empty()) {
            device->GetDeviceFn().vkUpdateDescriptorSets(device->GetNativeHandle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        }
        for (auto &[group, encoder] : encoders) {
            encoder->Commit();
        }
        encoders.clear();
        return true;
    }
    void VulkanDescriptorBatch::Reset()
    {
        encoders.clear();
        failed = false;
    }
} // namespace sky::aurora
