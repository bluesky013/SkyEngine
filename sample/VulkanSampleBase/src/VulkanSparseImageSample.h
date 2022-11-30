//
// Created by Zach Lee on 2022/11/30.
//

#pragma once

#include "VulkanSampleBase.h"
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/Shader.h>
#include <vulkan/VertexInput.h>
#include <vulkan/SparseImage.h>

namespace sky {
    class NativeWindow;

    class VulkanSparseImageSample : public VulkanSampleBase {
    public:
        VulkanSparseImageSample()  = default;
        ~VulkanSparseImageSample() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;
    private:
        void InitSparseImage();

        vk::GraphicsPipelinePtr pso;
        vk::PipelineLayoutPtr   pipelineLayout;
        vk::ShaderPtr           vs;
        vk::ShaderPtr           fs;
        vk::VertexInputPtr      vertexInput;
        vk::SparseImagePtr      sparseImage;
    };

} // namespace sky
