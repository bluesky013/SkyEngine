//
// Aurora Vulkan ResourceGroupLayout.
//

#include <VulkanResourceGroupLayout.h>
#include <VulkanDevice.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanResourceGroupLayout::VulkanResourceGroupLayout(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanResourceGroupLayout::~VulkanResourceGroupLayout()
    {
        if (setLayout != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyDescriptorSetLayout(device.GetNativeHandle(), setLayout, nullptr);
        }
    }

    bool VulkanResourceGroupLayout::Init(const Descriptor &desc)
    {
        bindings = desc.bindings;

        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        vkBindings.reserve(desc.bindings.size());

        for (const auto &b : desc.bindings) {
            if (b.flags & DescriptorBindingFlagBit::VARIABLE_COUNT) {
                LOG_E(TAG, "VARIABLE_COUNT bindings not supported in this change");
                return false;
            }
            VkDescriptorSetLayoutBinding lb = {};
            lb.binding         = b.binding;
            lb.descriptorType  = FromDescriptorType(b.type);
            lb.descriptorCount = b.count;
            lb.stageFlags      = FromShaderStageFlags(b.stages);
            vkBindings.push_back(lb);
        }

        VkDescriptorSetLayoutCreateInfo ci = {};
        ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = static_cast<uint32_t>(vkBindings.size());
        ci.pBindings    = vkBindings.data();

        const VkResult r = device.GetDeviceFn().vkCreateDescriptorSetLayout(
            device.GetNativeHandle(), &ci, nullptr, &setLayout);
        if (r != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateDescriptorSetLayout failed: %d", r);
            return false;
        }
        return true;
    }

} // namespace sky::aurora
