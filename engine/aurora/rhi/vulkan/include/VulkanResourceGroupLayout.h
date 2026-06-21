//
// Aurora Vulkan ResourceGroupLayout.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanResourceGroupLayout : public ResourceGroupLayout {
    public:
        explicit VulkanResourceGroupLayout(VulkanDevice &dev);
        ~VulkanResourceGroupLayout() override;

        bool Init(const Descriptor &desc);

        VkDescriptorSetLayout GetNativeHandle() const { return setLayout; }
        const std::vector<BindingDesc> &GetBindings() const { return bindings; }

    private:
        VulkanDevice         &device;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        std::vector<BindingDesc> bindings;
    };

} // namespace sky::aurora
