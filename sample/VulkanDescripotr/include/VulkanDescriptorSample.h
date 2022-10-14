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
        void UpdateDynamicBuffer();

        void OnMouseMove(int32_t x, int32_t y) override;
        void OnMouseWheel(int32_t wheelX, int32_t wheelY) override;

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
        drv::ImageViewPtr imageView0;
        drv::ImageViewPtr imageView1;
        drv::ImageViewPtr imageView2;

        drv::ImagePtr storageImage;
        drv::ImagePtr storageImageView;

        drv::BufferPtr uniformBuffer;
        drv::BufferPtr constantBuffer;
        drv::BufferPtr texelBuffer;
        drv::BufferPtr storageBuffer;

        drv::BufferViewPtr bufferView0;
        drv::BufferViewPtr bufferView1;

        struct Ubo {
            float x;
            float y;
            float scaleX;
            float scaleY; // padding
        };

        int32_t scale = 16;
        int32_t mouseX = 0;
        int32_t mouseY = 0;
    };

} // namespace sky
