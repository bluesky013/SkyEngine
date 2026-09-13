//
// Aurora Vulkan ResourceGroup.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/ShaderReflection.h>
#include <VulkanShader.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanDescriptorEncoder;

    class VulkanResourceGroup : public ResourceGroup {
    public:
        explicit VulkanResourceGroup(VulkanDevice &dev);
        ~VulkanResourceGroup() override;

        bool Init(const Descriptor &desc);

        std::unique_ptr<DescriptorEncoder> CreateEncoder() override;

        VkDescriptorSet GetNativeHandle() const { return set; }

    private:
        friend class VulkanDescriptorEncoder;

        struct BindingInfo {
            uint32_t          binding  = 0;
            uint32_t          count    = 0;
            uint32_t          slotBase = 0;
            VkDescriptorType  type     = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        };

        VulkanDevice                    *device = nullptr;
        CounterPtr<VulkanShader>         shader;       // keeps the derived set layout + template alive
        uint32_t                         mSetIndex = 0;
        std::vector<BindingInfo>         mBindings;    // binding -> packed slot layout
        std::vector<DescriptorWriteInfo> mWriteInfos;  // persistent packed write buffer
        bool                             mDirty = false;
        VkDescriptorPool                 pool = VK_NULL_HANDLE;
        VkDescriptorSet                  set  = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
