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

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();
        void UpdateDynamicBuffer();

        void OnMouseMove(int32_t x, int32_t y) override;
        void OnMouseWheel(int32_t wheelX, int32_t wheelY) override;

        vk::PipelineLayoutPtr      pipelineLayout;
        vk::DescriptorSetLayoutPtr descriptorSetLayout;
        vk::GraphicsPipelinePtr    pso;
        vk::DescriptorSetPtr       set;
        vk::DescriptorSetPoolPtr   setPool;
        vk::DescriptorSetBinderPtr setBinder;

        vk::ShaderPtr              vs;
        vk::ShaderPtr              fs;
        vk::VertexInputPtr         vertexInput;

        vk::SamplerPtr sampler;

        vk::ImagePtr     inputImage0;
        vk::ImageViewPtr imageView0;
        vk::ImageViewPtr imageView1;
        vk::ImageViewPtr imageView2;

        vk::ImagePtr storageImage;
        vk::ImagePtr storageImageView;

        vk::BufferPtr uniformBuffer;
        vk::BufferPtr constantBuffer;
        vk::BufferPtr texelBuffer;
        vk::BufferPtr storageBuffer;
        uint32_t       alignedSize = 0;

        vk::BufferViewPtr bufferView0;
        vk::BufferViewPtr bufferView1;

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
