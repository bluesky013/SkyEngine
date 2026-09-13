//
// Aurora Vulkan ResourceGroup.
//

#pragma once

#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/ShaderReflection.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace sky::aurora {

    class VulkanDevice;
    class VulkanShader;

    class VulkanResourceGroup : public ResourceGroup {
    public:
        explicit VulkanResourceGroup(VulkanDevice &dev);
        ~VulkanResourceGroup() override;

        bool Init(const Descriptor &desc);

        void Update(const std::vector<ResourceUpdateInfo> &writes) override;

        VkDescriptorSet GetNativeHandle() const { return set; }

    private:
        VulkanDevice               *device = nullptr;
        CounterPtr<VulkanShader>    shader;         // keeps the derived set layout alive
        std::vector<ShaderResource> setResources;   // this set's resources (type lookup for Update)
        VkDescriptorPool            pool = VK_NULL_HANDLE;
        VkDescriptorSet             set  = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
