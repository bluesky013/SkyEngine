//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"

namespace sky {
    class NativeWindow;

    class VulkanDescriptorSample : public VulkanSampleBase {
    public:
        VulkanDescriptorSample()  = default;
        ~VulkanDescriptorSample() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();

        void OnMouseMove(int32_t x, int32_t y) override;

        drv::PipelineLayoutPtr      pipelineLayout;
        drv::DescriptorSetLayoutPtr descriptorSetLayout;
        drv::GraphicsPipelinePtr    pso;
        drv::DescriptorSetPtr       set;
        drv::DescriptorSetPoolPtr   setPool;
        drv::DescriptorSetBinderPtr setBinder;

        drv::ShaderPtr              vs;
        drv::ShaderPtr              fs;
        drv::VertexInputPtr         vertexInput;

        drv::SamplerPtr sampler;

        drv::ImagePtr inputImage0;
        drv::ImageViewPtr inputImageView0;

        drv::ImagePtr inputImage1;
        drv::ImageViewPtr inputImageView1;

        drv::ImagePtr storageImage;
        drv::ImagePtr storageImageView;

        drv::BufferPtr uniformBuffer;
        drv::BufferPtr storageBuffer;
    };

} // namespace sky