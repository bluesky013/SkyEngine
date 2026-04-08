//
// Created by Zach Lee on 2026/4/7.
//

#pragma once

#include <aurora/rhi/Shader.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

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

    private:
        bool CreatePipelineLayout();

        VulkanDevice                       &device;
        CounterPtr<VulkanShaderFunction>    vertexFunction;
        CounterPtr<VulkanShaderFunction>    fragmentFunction;
        CounterPtr<VulkanShaderFunction>    computeFunction;
        VkPipelineLayout                    layout = VK_NULL_HANDLE;
    };

} // namespace sky::aurora