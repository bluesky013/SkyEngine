#include <VulkanResourceGroup.h>

#include <VulkanConversion.h>
#include <VulkanDescriptorEncoder.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>

#include <map>

namespace sky::aurora {
    VulkanResourceGroup::VulkanResourceGroup(VulkanDevice &dev) : device(&dev)
    {
    }
    VulkanResourceGroup::~VulkanResourceGroup()
    {
        if (pool != VK_NULL_HANDLE) {
            device->GetDeviceFn().vkDestroyDescriptorPool(device->GetNativeHandle(), pool, nullptr);
        }
    }
    const Device *VulkanResourceGroup::GetDevice() const noexcept
    {
        return device;
    }
    bool VulkanResourceGroup::Init(const Descriptor &desc)
    {
        if (desc.shader == nullptr || desc.shader->GetDevice() != device) {
            LOG_E("AuroraVulkan", "ResourceGroup shader has an unknown or different device");
            return false;
        }
        if (!InitParameterLayout(desc)) {
            return false;
        }
        shader            = static_cast<VulkanShader *>(desc.shader);
        mSetIndex         = desc.set;
        const auto &plan  = shader->GetBindingPlan();
        const auto  found = plan.logicalBindings.find(desc.set);
        if (found == plan.logicalBindings.end()) {
            return true;
        }
        mBindings        = found->second;
        const auto count = plan.logicalSlotCounts.at(desc.set);
        mWriteInfos.resize(count);
        mAssigned.resize(count, false);
        mBuffers.resize(count);
        mImages.resize(count);
        mSamplers.resize(count);
        std::vector<VkDescriptorSetLayout>   layouts;
        std::map<VkDescriptorType, uint32_t> counts;
        for (const auto &[set, native] : plan.sets) {
            if (native.logicalSet != desc.set) {
                continue;
            }
            const auto layout = shader->GetDescriptorSetLayout(set);
            if (layout == VK_NULL_HANDLE) {
                return false;
            }
            mSets.push_back({set, VK_NULL_HANDLE});
            layouts.push_back(layout);
            for (const auto &binding : native.bindings) {
                const auto type = FromShaderResourceType(binding.type);
                if (type == VK_DESCRIPTOR_TYPE_MAX_ENUM || binding.count > UINT32_MAX - counts[type]) {
                    return false;
                }
                counts[type] += binding.count;
            }
        }
        if (mSets.empty()) {
            return false;
        }
        std::vector<VkDescriptorPoolSize> sizes;
        for (const auto &[type, size] : counts) {
            sizes.push_back({type, size});
        }
        VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        info.maxSets       = static_cast<uint32_t>(mSets.size());
        info.poolSizeCount = static_cast<uint32_t>(sizes.size());
        info.pPoolSizes    = sizes.data();
        const auto &fn     = device->GetDeviceFn();
        auto        status = fn.vkCreateDescriptorPool(device->GetNativeHandle(), &info, nullptr, &pool);
        if (status != VK_SUCCESS) {
            LOG_E("AuroraVulkan", "vkCreateDescriptorPool failed: %d", status);
            return false;
        }
        std::vector<VkDescriptorSet> handles(layouts.size());
        VkDescriptorSetAllocateInfo  allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocation.descriptorPool     = pool;
        allocation.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocation.pSetLayouts        = layouts.data();
        status                        = fn.vkAllocateDescriptorSets(device->GetNativeHandle(), &allocation, handles.data());
        if (status != VK_SUCCESS) {
            LOG_E("AuroraVulkan", "vkAllocateDescriptorSets failed: %d", status);
            return false;
        }
        for (size_t i = 0; i < handles.size(); ++i) {
            mSets[i].handle = handles[i];
        }
        return true;
    }
    std::unique_ptr<DescriptorEncoder> VulkanResourceGroup::CreateEncoder()
    {
        if (IsParameterSnapshot()) {
            return nullptr;
        }
        return std::make_unique<VulkanDescriptorEncoder>(*this);
    }
    bool VulkanResourceGroup::Bind(VkCommandBuffer     command,
                                   VkPipelineBindPoint point,
                                   VkPipelineLayout    layout,
                                   uint32_t            logicalSet,
                                   uint32_t            offsetCount,
                                   const uint32_t     *offsets) const
    {
        const auto    &plan     = shader->GetBindingPlan();
        const auto     found    = plan.logicalDynamicCounts.find(mSetIndex);
        const uint32_t expected = found == plan.logicalDynamicCounts.end() ? 0 : found->second;
        if (logicalSet != mSetIndex || layout != shader->GetPipelineLayout() || offsetCount != expected || (offsetCount && offsets == nullptr)) {
            return false;
        }
        for (const auto assigned : mAssigned) {
            if (!assigned) {
                return false;
            }
        }
        for (const auto &binding : mBindings) {
            const bool uniform = binding.type == ShaderResourceType::UNIFORM_BUFFER_DYNAMIC;
            if (!uniform && binding.type != ShaderResourceType::STORAGE_BUFFER_DYNAMIC) {
                continue;
            }
            const auto alignment =
                uniform ? device->GetLimits().minUniformBufferOffsetAlignment : device->GetLimits().minStorageBufferOffsetAlignment;
            for (uint32_t element = 0; element < binding.count; ++element) {
                const auto  slot    = binding.slotBase + element;
                const auto  dynamic = offsets[binding.dynamicBase + element];
                const auto &range   = mWriteInfos[slot].buffer;
                const auto  size    = mBuffers[slot]->GetSize();
                if ((alignment && dynamic % alignment) || range.offset > size || dynamic > size - range.offset ||
                    range.range > size - range.offset - dynamic) {
                    return false;
                }
            }
        }
        for (const auto &set : mSets) {
            std::vector<uint32_t> nativeOffsets;
            for (const auto index : plan.sets.at(set.index).dynamicOffsets) {
                nativeOffsets.push_back(offsets[index]);
            }
            device->GetDeviceFn().vkCmdBindDescriptorSets(command, point, layout, set.index, 1, &set.handle,
                                                          static_cast<uint32_t>(nativeOffsets.size()), nativeOffsets.data());
        }
        return true;
    }
} // namespace sky::aurora
