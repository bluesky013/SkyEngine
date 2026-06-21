//
// Aurora Vulkan PipelineLayout.
//

#pragma once

#include <aurora/rhi/PipelineLayout.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanPipelineLayout : public PipelineLayout {
    public:
        explicit VulkanPipelineLayout(VulkanDevice &dev);
        ~VulkanPipelineLayout() override;

        bool Init(const Descriptor &desc);

        VkPipelineLayout GetNativeHandle() const { return layout; }

    private:
        VulkanDevice    &device;
        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
