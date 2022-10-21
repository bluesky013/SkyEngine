//
// Created by Zach Lee on 2022/6/16.
//

#pragma once

#include "VulkanSampleBase.h"
#include <core/math/Vector4.h>
#include <future>

namespace sky {
    class NativeWindow;

    struct Particle {
        Vector4 pos;
        Vector4 dir;
    };

    struct ParticleRender {
        Vector4 pos;
    };

    struct FrameData {
        uint32_t frameIndex;
        float delta;
    };

    struct ParticleSystem {
        drv::BufferPtr         input;
        drv::BufferPtr         output;
        drv::BufferPtr         ubo;
        drv::VertexInputPtr    vertexInput;
        drv::VertexAssemblyPtr vertexAssembly;
    };

    class VulkanMemoryAliasing : public VulkanSampleBase {
    public:
        VulkanMemoryAliasing()  = default;
        ~VulkanMemoryAliasing() = default;

        void Tick(float delta) override;

        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();
        void SetupPass();

        static constexpr uint32_t DISPATCH     = 10;
        static constexpr uint32_t DISPATCH_GRP = 256;
        static constexpr uint32_t PARTICLE_NUM = DISPATCH_GRP * DISPATCH;

        drv::DescriptorSetPoolPtr setPool;

        drv::PipelineLayoutPtr   gfxLayout;
        drv::GraphicsPipelinePtr gfxPipeline;

        drv::PipelineLayoutPtr  compLayout;
        drv::ComputePipelinePtr compPipeline;

        drv::PipelineLayoutPtr   compositeLayout;
        drv::GraphicsPipelinePtr compositePipeline;

        drv::DescriptorSetPtr gfxSet;
        drv::DescriptorSetPtr compSet;
        drv::DescriptorSetPtr fullScreenSet;
        drv::DescriptorSetPtr compositeSet;

        drv::DescriptorSetBinderPtr gfxBinder;
        drv::DescriptorSetBinderPtr compBinder;
        drv::DescriptorSetBinderPtr fullScreenSetBinder;
        drv::DescriptorSetBinderPtr compositeSetBinder;

        drv::ShaderPtr vs;
        drv::ShaderPtr fs;
        drv::ShaderPtr cs;
        drv::ShaderPtr fullScreenVs;
        drv::ShaderPtr fullScreenFs;
        drv::ShaderPtr compositeFS;

        drv::SamplerPtr   sampler;
        drv::ImageViewPtr view;
        drv::ImagePtr     image;

        drv::RenderPassPtr sampledPass;

        drv::ImagePtr            rasterTarget;
        drv::ImageViewPtr        rasterTargetView;
        drv::FrameBufferPtr      rasterFb;

        drv::ImagePtr            fullScreenTarget;
        drv::ImageViewPtr        fullScreenTargetView;
        drv::FrameBufferPtr      fullScreenFb;
        drv::GraphicsPipelinePtr fullScreenPso;
        drv::VertexInputPtr      fullScreenInput;

        drv::GraphicsPipelinePtr compositePso;
        VmaAllocation alloc;

        ParticleSystem particleSystem;
    };

} // namespace sky
