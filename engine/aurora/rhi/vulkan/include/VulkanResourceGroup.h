//
// Aurora Vulkan ResourceGroup.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanResourceGroupLayout;

    class VulkanResourceGroup : public ResourceGroup {
    public:
        explicit VulkanResourceGroup(VulkanDevice &dev);
        ~VulkanResourceGroup() override;

        bool Init(const Descriptor &desc);

        void Update(const std::vector<ResourceUpdateInfo> &writes) override;

        VkDescriptorSet GetNativeHandle() const { return set; }

    private:
        VulkanDevice               *device     = nullptr;
        VulkanResourceGroupLayout  *layout     = nullptr;
        VkDescriptorPool            pool       = VK_NULL_HANDLE;
        VkDescriptorSet             set        = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
