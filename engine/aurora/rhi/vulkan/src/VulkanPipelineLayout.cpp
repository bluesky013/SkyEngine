//
// Aurora Vulkan PipelineLayout.
//

#include <VulkanPipelineLayout.h>
#include <VulkanResourceGroupLayout.h>
#include <VulkanDevice.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanPipelineLayout::VulkanPipelineLayout(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanPipelineLayout::~VulkanPipelineLayout()
    {
        if (layout != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyPipelineLayout(device.GetNativeHandle(), layout, nullptr);
        }
    }

    bool VulkanPipelineLayout::Init(const Descriptor &desc)
    {
        if (desc.groups.size() > MAX_RESOURCE_GROUPS) {
            LOG_E(TAG, "PipelineLayout has %zu groups; max is %u", desc.groups.size(), MAX_RESOURCE_GROUPS);
            return false;
        }

        std::vector<VkDescriptorSetLayout> setLayouts;
        setLayouts.reserve(desc.groups.size());
        for (auto *g : desc.groups) {
            if (g != nullptr) {
                setLayouts.push_back(static_cast<VulkanResourceGroupLayout *>(g)->GetNativeHandle());
            } else {
                setLayouts.push_back(VK_NULL_HANDLE);
            }
        }

        std::vector<VkPushConstantRange> pcRanges;
        pcRanges.reserve(desc.pushConstants.size());
        for (const auto &pc : desc.pushConstants) {
            VkPushConstantRange r = {};
            r.stageFlags = FromShaderStageFlags(pc.stageFlags);
            r.offset     = pc.offset;
            r.size       = pc.size;
            pcRanges.push_back(r);
        }

        VkPipelineLayoutCreateInfo ci = {};
        ci.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ci.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        ci.pSetLayouts            = setLayouts.data();
        ci.pushConstantRangeCount = static_cast<uint32_t>(pcRanges.size());
        ci.pPushConstantRanges    = pcRanges.data();

        const VkResult r = device.GetDeviceFn().vkCreatePipelineLayout(
            device.GetNativeHandle(), &ci, nullptr, &layout);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreatePipelineLayout failed: %d", r);
            return false;
        }
        return true;
    }

} // namespace sky::aurora
