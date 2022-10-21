//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"
#include <future>

namespace sky {
    class NativeWindow;

    struct UploadEvent {
        virtual void OnBufferUploaded(drv::BufferPtr &buffer) = 0;
    };

    struct Object {
        std::atomic_bool            isReady = false;
        std::future<void>           future;
        drv::DescriptorSetBinderPtr setBinder;
    };

    class VulkanAsyncUploadSample : public VulkanSampleBase {
    public:
        VulkanAsyncUploadSample()  = default;
        ~VulkanAsyncUploadSample() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();

        drv::PipelineLayoutPtr      pipelineLayout;
        drv::DescriptorSetLayoutPtr descriptorSetLayout;
        drv::GraphicsPipelinePtr    pso;
        drv::DescriptorSetPtr       set;
        drv::DescriptorSetPoolPtr   setPool;

        drv::ShaderPtr      vs;
        drv::ShaderPtr      fs;
        drv::VertexInputPtr vertexInput;
        drv::SamplerPtr     sampler;
        drv::ImageViewPtr   view;
        drv::ImagePtr       image;

        Object object;
    };

} // namespace sky
