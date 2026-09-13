//
// Created by Zach Lee on 2026/4/7.
//

#pragma once

#include <aurora/rhi/Shader.h>
#include <vulkan/vulkan.h>

#include <map>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    // Packed descriptor write slot (union of buffer/image info). The per-set
    // VkDescriptorUpdateTemplate reads these with a fixed stride.
    union DescriptorWriteInfo {
        VkDescriptorBufferInfo buffer;
        VkDescriptorImageInfo  image;
    };

    class VulkanShaderFunction : public ShaderFunction {
    public:
        explicit VulkanShaderFunction(VulkanDevice &dev);
        ~VulkanShaderFunction() override;

        bool Init(const Descriptor &desc);

        VkShaderModule     GetNativeHandle() const { return shaderModule; }
        ShaderStageFlagBit GetStage() const { return stage; }

    private:
        VulkanDevice       &device;
        VkShaderModule      shaderModule = VK_NULL_HANDLE;
        ShaderStageFlagBit  stage        = ShaderStageFlagBit::VS;
    };

    class VulkanShader : public Shader {
    public:
        explicit VulkanShader(VulkanDevice &dev);
        ~VulkanShader() override;

        bool Init(const Descriptor &desc);

        VulkanShaderFunction *GetVertexFunction() const { return vertexFunction.Get(); }
        VulkanShaderFunction *GetFragmentFunction() const { return fragmentFunction.Get(); }
        VulkanShaderFunction *GetComputeFunction() const { return computeFunction.Get(); }
        VkPipelineLayout GetPipelineLayout() const { return layout; }
        const ShaderSpecialization &GetSpecialization() const { return specialization; }
        const ShaderReflection &GetReflection() const { return reflection; }

        // Per-set descriptor set layout derived from reflection, queried by
        // real set index (may contain holes). Returns VK_NULL_HANDLE if absent.
        VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) const;

        // Per-set descriptor update template (Vulkan 1.1 core), queried by real
        // set index. Returns VK_NULL_HANDLE if absent or creation failed.
        VkDescriptorUpdateTemplate GetDescriptorUpdateTemplate(uint32_t set) const;

    private:
        bool CreatePipelineLayout();

        VulkanDevice                       &device;
        CounterPtr<VulkanShaderFunction>    vertexFunction;
        CounterPtr<VulkanShaderFunction>    fragmentFunction;
        CounterPtr<VulkanShaderFunction>    computeFunction;
        ShaderReflection                    reflection;
        ShaderSpecialization                specialization;
        std::map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;       // set index -> layout
        std::map<uint32_t, VkDescriptorUpdateTemplate> descriptorUpdateTemplates; // set index -> template
        VkPipelineLayout                    layout = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
