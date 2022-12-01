//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"

namespace sky {
    class NativeWindow;

    class VulkanBindlessSample : public VulkanSampleBase {
    public:
        VulkanBindlessSample()  = default;
        ~VulkanBindlessSample() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;

        static constexpr uint32_t IMAGE_NUM = 4;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();

        void InitFeature() override;

        vk::PipelineLayoutPtr      pipelineLayout;
        vk::DescriptorSetLayoutPtr descriptorSetLayout;
        vk::GraphicsPipelinePtr    pso;
        vk::DescriptorSetPtr       set;
        vk::DescriptorSetPoolPtr   setPool;
        vk::DescriptorSetBinderPtr setBinder;

        vk::ShaderPtr      vs;
        vk::ShaderPtr      fs;
        vk::VertexInputPtr vertexInput;

        vk::VertexAssemblyPtr vertexAssembly;
        vk::BufferPtr         vertexBuffer;
        vk::BufferPtr         materialBuffer;
        vk::BufferPtr         indirectBuffer;
        vk::SamplerPtr        sampler;
        vk::ImagePtr          images[IMAGE_NUM];
        vk::ImageViewPtr      imageViews[IMAGE_NUM];
    };

} // namespace sky
