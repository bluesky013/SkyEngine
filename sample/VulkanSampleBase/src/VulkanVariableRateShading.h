//
// Created by Zach Lee on 2023/3/3.
//

#pragma once

#include "VulkanSampleBase.h"
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Shader.h>
#include <vulkan/VertexInput.h>

namespace sky {
    class NativeWindow;

    class VulkanVariableRateShading : public VulkanSampleBase {
    public:
        VulkanVariableRateShading()  = default;
        ~VulkanVariableRateShading() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;
    private:
        void InitFeature() override { deviceInfo.feature.variableRateShading = true; }
        void SetupRenderPass();
        void SetupFrameBuffer();

        vk::GraphicsPipelinePtr pso1;
        vk::GraphicsPipelinePtr pso2;
        vk::PipelineLayoutPtr   pipelineLayout;
        vk::ShaderPtr           vs;
        vk::ShaderPtr           fs;
        vk::VertexInputPtr      vertexInput;

        vk::ImagePtr            shadingRateImage;
        vk::ImageViewPtr        shadingRateImageView;
        vk::RenderPassPtr       shadingRatePass;
        std::vector<vk::FrameBufferPtr> shadingRateFbs;
    };

} // namespace sky
