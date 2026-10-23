#pragma once

#include <VulkanShader.h>
#include <aurora/rhi/Buffer.h>
#include <aurora/rhi/Image.h>
#include <aurora/rhi/ResourceGroup.h>
#include <aurora/rhi/Sampler.h>

namespace sky::aurora {
    class VulkanDevice;
    class VulkanDescriptorEncoder;
    class VulkanDescriptorBatch;
    class VulkanResourceGroup : public ResourceGroup {
    public:
        explicit VulkanResourceGroup(VulkanDevice &dev);
        ~VulkanResourceGroup() override;
        bool          Init(const Descriptor &desc);
        const Device *GetDevice() const noexcept override;
        using ResourceGroup::CreateEncoder;
        std::unique_ptr<DescriptorEncoder> CreateEncoder() override;
        bool                               Bind(VkCommandBuffer     command,
                                                VkPipelineBindPoint point,
                                                VkPipelineLayout    layout,
                                                uint32_t            logicalSet,
                                                uint32_t            offsetCount,
                                                const uint32_t     *offsets) const;

    private:
        friend class VulkanDescriptorEncoder;
        friend class VulkanDescriptorBatch;
        struct NativeSet {
            uint32_t        index  = 0;
            VkDescriptorSet handle = VK_NULL_HANDLE;
        };
        VulkanDevice                     *device = nullptr;
        CounterPtr<VulkanShader>          shader;
        uint32_t                          mSetIndex = 0;
        std::vector<VulkanPlannedBinding> mBindings;
        std::vector<DescriptorWriteInfo>  mWriteInfos;
        std::vector<bool>                 mAssigned;
        std::vector<BufferPtr>            mBuffers;
        std::vector<ImagePtr>             mImages;
        std::vector<SamplerPtr>           mSamplers;
        std::vector<NativeSet>            mSets;
        VkDescriptorPool                  pool = VK_NULL_HANDLE;
    };
} // namespace sky::aurora
