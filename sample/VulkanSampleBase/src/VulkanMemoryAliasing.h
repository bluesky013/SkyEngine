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
        vk::BufferPtr         input;
        vk::BufferPtr         output;
        vk::BufferPtr         ubo;
        vk::VertexInputPtr    vertexInput;
        vk::VertexAssemblyPtr vertexAssembly;
    };

    class VulkanMemoryAliasing : public VulkanSampleBase {
    public:
        VulkanMemoryAliasing()  = default;
        ~VulkanMemoryAliasing() = default;

        void OnKeyUp(KeyButtonType button) override;

        void OnTick(float delta) override;
        void OnStart() override;
        void OnStop() override;

    private:
        void SetupPso();
        void SetupDescriptorSet();
        void SetupResources();
        void SetupPass();
        void ResetParticlePool();

        static constexpr uint32_t DISPATCH     = 10;
        static constexpr uint32_t DISPATCH_GRP = 256;
        static constexpr uint32_t PARTICLE_NUM = DISPATCH_GRP * DISPATCH;

        vk::DescriptorSetPoolPtr setPool;

        vk::PipelineLayoutPtr   gfxLayout;
        vk::GraphicsPipelinePtr gfxPipeline;

        vk::PipelineLayoutPtr  compLayout;
        vk::ComputePipelinePtr compPipeline;

        vk::PipelineLayoutPtr   compositeLayout;
        vk::GraphicsPipelinePtr compositePipeline;

        vk::DescriptorSetPtr gfxSet;
        vk::DescriptorSetPtr compSet;
        vk::DescriptorSetPtr fullScreenSet;
        vk::DescriptorSetPtr compositeSet;

        vk::DescriptorSetBinderPtr gfxBinder;
        vk::DescriptorSetBinderPtr compBinder;
        vk::DescriptorSetBinderPtr fullScreenSetBinder;
        vk::DescriptorSetBinderPtr compositeSetBinder;

        vk::ShaderPtr vs;
        vk::ShaderPtr fs;
        vk::ShaderPtr cs;
        vk::ShaderPtr fullScreenVs;
        vk::ShaderPtr fullScreenFs;
        vk::ShaderPtr compositeFS;

        vk::SamplerPtr   sampler;
        vk::ImageViewPtr view;
        vk::ImagePtr     image;

        vk::RenderPassPtr sampledPass;

        vk::ImagePtr       rasterTarget;
        vk::ImageViewPtr   rasterTargetView;
        vk::FrameBufferPtr rasterFb;

        vk::ImagePtr            fullScreenTarget;
        vk::ImageViewPtr        fullScreenTargetView;
        vk::FrameBufferPtr      fullScreenFb;
        vk::GraphicsPipelinePtr fullScreenPso;
        vk::VertexInputPtr      fullScreenInput;

        vk::GraphicsPipelinePtr compositePso;
        VmaAllocation           alloc;

        std::unique_ptr<ParticleSystem> particleSystem;
    };

} // namespace sky
