//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"
#include <future>

namespace sky {
    class NativeWindow;

    struct UploadEvent {
        virtual void OnBufferUploaded(vk::BufferPtr &buffer) = 0;
    };

    struct Object {
        rhi::TransferTaskHandle uploadHandle;
        vk::DescriptorSetBinderPtr setBinder;
    };

    class VulkanAsyncUploadSample : public VulkanSampleBase {
    public:
        VulkanAsyncUploadSample()  = default;
        ~VulkanAsyncUploadSample() = default;

        void OnTick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();

        vk::PipelineLayoutPtr      pipelineLayout;
        vk::DescriptorSetLayoutPtr descriptorSetLayout;
        vk::GraphicsPipelinePtr    pso;
        vk::DescriptorSetPtr       set;
        vk::DescriptorSetPoolPtr   setPool;

        vk::ShaderPtr      vs;
        vk::ShaderPtr      fs;
        vk::VertexInputPtr vertexInput;
        vk::SamplerPtr     sampler;
        vk::ImageViewPtr   view;
        vk::ImagePtr       image;

        Object object;
    };

} // namespace sky
