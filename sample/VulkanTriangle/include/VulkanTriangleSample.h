//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Shader.h>
#include <vulkan/VertexInput.h>

namespace sky {
    class NativeWindow;

    class VulkanTriangleSample : public VulkanSampleBase {
    public:
        VulkanTriangleSample()  = default;
        ~VulkanTriangleSample() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;
    private:
        vk::GraphicsPipelinePtr pso;
        vk::PipelineLayoutPtr   pipelineLayout;
        vk::ShaderPtr           vs;
        vk::ShaderPtr           fs;
        vk::VertexInputPtr      vertexInput;
    };

} // namespace sky
