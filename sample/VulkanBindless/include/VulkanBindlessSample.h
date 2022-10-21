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

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

        static constexpr uint32_t IMAGE_NUM = 4;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();

        drv::PipelineLayoutPtr      pipelineLayout;
        drv::DescriptorSetLayoutPtr descriptorSetLayout;
        drv::GraphicsPipelinePtr    pso;
        drv::DescriptorSetPtr       set;
        drv::DescriptorSetPoolPtr   setPool;
        drv::DescriptorSetBinderPtr setBinder;

        drv::ShaderPtr      vs;
        drv::ShaderPtr      fs;
        drv::VertexInputPtr vertexInput;

        drv::VertexAssemblyPtr vertexAssembly;
        drv::BufferPtr         vertexBuffer;
        drv::BufferPtr         materialBuffer;
        drv::BufferPtr         indirectBuffer;
        drv::SamplerPtr        sampler;
        drv::ImagePtr          images[IMAGE_NUM];
        drv::ImageViewPtr      imageViews[IMAGE_NUM];
    };

} // namespace sky
