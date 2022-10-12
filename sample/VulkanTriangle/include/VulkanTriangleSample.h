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
        drv::GraphicsPipelinePtr pso;
        drv::PipelineLayoutPtr   pipelineLayout;
        drv::ShaderPtr           vs;
        drv::ShaderPtr           fs;
        drv::VertexInputPtr      vertexInput;
    };

} // namespace sky