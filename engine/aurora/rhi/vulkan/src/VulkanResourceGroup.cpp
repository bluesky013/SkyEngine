//
// Aurora Vulkan ResourceGroup.
//

#include <VulkanResourceGroup.h>
#include <VulkanDescriptorEncoder.h>
#include <VulkanDevice.h>
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
        if (desc.shader == nullptr) {
            LOG_E(TAG, "ResourceGroup requires a non-null shader");
            return false;
        }
        shader    = static_cast<VulkanShader *>(desc.shader);
        mSetIndex = desc.set;

        // snapshot this set's bindings + packed slot layout (same reflection
        // order as the shader's set layout / update template)
        mBindings.clear();
        uint32_t slotBase = 0;
        for (const auto &res : shader->GetReflection().resources) {
            if (res.set != desc.set) {
                continue;
            }
            BindingInfo info{};
            info.binding  = res.binding;
            info.count    = res.count;
            info.slotBase = slotBase;
            info.type     = FromShaderResourceType(res.type);
            mBindings.push_back(info);
            slotBase += res.count;
        }
        mWriteInfos.resize(slotBase);

        // build a tight descriptor pool sized exactly for this set.
        std::unordered_map<VkDescriptorType, uint32_t> counts;
        for (const auto &b : mBindings) {
            counts[b.type] += b.count;
        }
        std::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve(counts.size());
        for (const auto &kv : counts) {
            sizes.push_back({kv.first, kv.second});
        }
        if (sizes.empty()) {
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

        VkDescriptorSetLayout setLayout = shader->GetDescriptorSetLayout(desc.set);
        if (setLayout == VK_NULL_HANDLE) {
            LOG_E(TAG, "shader has no descriptor set layout for set %u", desc.set);
            return false;
        }

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

    std::unique_ptr<DescriptorEncoder> VulkanResourceGroup::CreateEncoder()
    {
        return std::make_unique<VulkanDescriptorEncoder>(*this);
    }

} // namespace sky::aurora
